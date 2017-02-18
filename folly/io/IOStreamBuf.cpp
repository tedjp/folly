#include "../Likely.h"
#include "IOStreamBuf.h"

namespace folly {

IOStreamBuf::IOStreamBuf(const std::shared_ptr<folly::IOBuf>& buf)
  : basic_streambuf<uint8_t>(),
    head_(buf),
    gcur_(buf.get()) {
  setg(gcur->data(); gcur->data(); gcur->tail());
}

void IOStreamBuf::swap(IOStreamBuf& rhs) {
  basic_streambuf<uint8_t>::swap(rhs);
  std::swap(head_, rhs.head_);
  std::swap(gcur_, rhs.gcur_);
}

// This is called either to rewind the get area (because eback() == egptr())
// or to attempt to put back a non-matching character (which we disallow
// on non-mutable IOBufs).
int_type IOStreamBuf::pbackfail(int_type c = Traits::eof()) override {
  if (egptr() != eback())
    return Traits::eof();

  if (gcur_ == head.get()) {
    // Already at beginning of first IOBuf
    return Traits::eof();
  }

  // Find the next preceding non-empty IOBuf, back to head_
  // Ensure the object state is not modified unless an earlier input sequence
  // can be found.
  IOBuf *prev = gcur_;
  do {
    prev = prev->prev();
  } while (prev->length() == 0 && prev != head_->get());

  if (UNLIKELY(prev->length() == 0))
    return Traits::eof();

  // Check whether c matches potential *gptr() before updating pointers
  if (c != Traits::to_int_type(prev->tail()[-1]))
    return Traits::eof();

  gcur_ = prev;

  setg(gcur_->data(), gcur_->tail() - 1, gcur_->tail());

  return Traits::to_int_type(*gptr());
}

int_type IOStreamBuf::underflow() {
  // public methods only call underflow() when gptr() >= egptr()
  // (but it's not an error to call underflow when gptr() < egptr())
  if (UNLIKELY(gptr() < egptr()))
    return Traits::to_int_type(*gptr());

  // Also handles non-chained
  IOBuf *next = gcur_->next();
  if (next == head_)
    return Traits::eof();

  gcur_ = next;
  setg(gcur_->data(), gcur_->data(), gcur_->tail());

  return Traits::to_int_type(*gptr());
}

pos_type IOStreamBuf::current_position() const {
  pos_type pos = 0;

  for (const IOBuf *buf = head_->get(); buf != gcur_; buf = buf->next())
    pos += buf->length();

  return pos + (gptr() - eback());
}

pos_type IOStreamBuf::seekoff(off_type off, std::ios_base::seekdir way, std::ios_base::openmode which) {
  static constexpr pos_type badoff = static_cast<pos_type>(static_cast<off_type>(-1));

  if ((which & std::ios_base::in) != std::ios_base::in)
    return badoff;

  if (way == std::ios_base::beg) {
    if (UNLIKELY(off < 0))
      return badoff;

    IOBuf *buf = head_->get();

    auto remaining_offset = off;

    while (remaining_offset > buf->length()) {
      remaining_offset -= buf->length();
      buf = buf->next();
      if (buf == head_->get())
        return badoff;
      }

    gcur_ = buf;
    setg(gcur_->data(), gcur->data() + remaining_offset, gcur->tail());

    return pos_type(off);
  }

  if (way == std::ios_base::end) {
    if (UNLIKELY(off > 0))
      return badoff;

    IOBuf *buf = head->prev();

    // Work with positive offset working back from the last tail()
    auto remaining_offset = 0 - off;

    while (remaining_offset > buf->length()) {
      remaining_offset -= buf->length();
      buf = buf->prev();
      if (buf == head_->get() && off > buf->length())
        return badoff;
    }

    gcur_ = buf;
    setg(gcur->data(), gcur->tail() - remaining_offset, gcur->tail());

    return current_position();
  }

  if (way == std::ios_base::cur) {
    if (UNLIKELY(off == 0))
      return current_position();

    IOBuf *buf = gcur_;

    auto remaining_offset = off;

    if (remaining_offset < 0) {
      // backwards; use as positive distance backward
      remaining_offset = 0 - remaining_offset;

      if (remaining_offset < gptr() - eback()) {
        // In the same IOBuf
        setg(gcur_->data(), gcur_->data() + gptr() - eback() - remaining_offset, gcur_->tail());
        return current_position();
      }

      remaining_offset -= gptr() - eback();

      do {
        buf = buf->prev();

        if (buf == head_->get() && remaining_offset > buf->length())
          return badoff; // position precedes start of data

        remaining_offset -= buf->length();
      } while (remaining_offset > buf->length());

      gcur_ = buf;
      setg(gcur_->data(), gcur_->tail() - remaining_offset, gcur_->tail());
      return current_position();
    }

    assert(remaining_offset > 0);

    if (remaining_offset < egptr() - gptr()) {
      assert(egptr() == gcur_->tail());
      setg(gcur_->data(), gptr() + remaining_offset, gcur_->tail());
      return current_position();
    }

    remaining_offset -= egptr() - gptr();

    for (buf = buf->next();
         buf != head_->get();
         buf = buf->next()) {
      if (remaining_offset < buf->length()) {
        gcur_ = buf;
        setg(gcur_->data(), gcur_->data() + remaining_offset, gcur_->tail());
        return current_position();
      }

      remaining_offset -= buf->length();
    }

    return badoff;
  }

  return badoff;
}

pos_type IOStreamBuf::seekpos(pos_type pos, std::ios_base::openmode which) {
  return seekoff(off_type(pos), std::ios_base::beg, which);
}

streamsize IOStreamBuf::showmanyc() {
  streamsize s = egptr() - gptr();

  for (const IOBuf *buf = gcur_->next(); buf != head_->get(); buf = buf->next())
    s += buf->length();

  return s;
}

std::streamsize IOStreamBuf::xsgetn(uint8_t* s, std::streamsize count) {
  std::streamsize copied = 0;

  std::streamsize n = std::min(egptr() - gptr(), count);
  memcpy(s + copied, gptr(), n);
  count -= n;
  copied += n;

  for (const IOBuf *buf = gcur_->next(); buf != head_->get() && count > 0; buf = buf->next()) {
    n = std::min(buf->length(), count);
    memcpy(s + copied, buf->data(), n);
    count -= n;
    copied += n;
  }

  gbump(copied);

  return copied;
}

} // namespace

// vim: ts=2 sw=2 tw=80 et ai
