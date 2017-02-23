#include <folly/Likely.h>

namespace folly {

template <typename T>
IOStreamBuf<T>::IOStreamBuf(const folly::IOBuf* head)
  : std::basic_streambuf<T>(),
    head_(head),
    gcur_(head) {
  csetg(gcur_->data(), gcur_->data(), gcur_->tail());
}

template <typename T>
void IOStreamBuf<T>::swap(IOStreamBuf<T>& rhs) {
  std::basic_streambuf<T>::swap(rhs);
  std::swap(head_, rhs.head_);
  std::swap(gcur_, rhs.gcur_);
}

template <typename T>
const typename IOStreamBuf<T>::pos_type IOStreamBuf<T>::badoff =
  static_cast<typename IOStreamBuf<T>::pos_type>(
    static_cast<typename IOStreamBuf<T>::off_type>(-1));

// This is called either to rewind the get area (because eback() == egptr())
// or to attempt to put back a non-matching character (which we disallow
// on non-mutable IOBufs).
template <typename T>
typename IOStreamBuf<T>::int_type IOStreamBuf<T>::pbackfail(int_type c) {
  if (this->egptr() != this->eback())
    return traits_type::eof();

  if (gcur_ == head_) {
    // Already at beginning of first IOBuf
    return traits_type::eof();
  }

  // Find the next preceding non-empty IOBuf, back to head_
  // Ensure the object state is not modified unless an earlier input sequence
  // can be found.
  const IOBuf* prev = gcur_;
  do {
    prev = prev->prev();
  } while (prev->length() == 0 && prev != head_);

  // XXX: Needs modification when sizeof(T) > 1
  if (UNLIKELY(prev->length() < sizeof(T)))
    return traits_type::eof();

  // Check whether c matches potential *gptr() before updating pointers
  if (c != traits_type::to_int_type(prev->tail()[-1]))
    return traits_type::eof();

  gcur_ = prev;

  csetg(gcur_->data(), gcur_->tail() - 1, gcur_->tail());

  return traits_type::to_int_type(*this->gptr());
}

template <typename T>
typename IOStreamBuf<T>::int_type IOStreamBuf<T>::underflow() {
  // public methods only call underflow() when gptr() >= egptr()
  // (but it's not an error to call underflow when gptr() < egptr())
  if (UNLIKELY(this->gptr() < this->egptr()))
    return traits_type::to_int_type(*this->gptr());

  // Also handles non-chained
  const IOBuf* next = gcur_->next();
  if (next == head_)
    return traits_type::eof();

  gcur_ = next;
  csetg(gcur_->data(), gcur_->data(), gcur_->tail());

  return traits_type::to_int_type(*this->gptr());
}

template <typename T>
typename IOStreamBuf<T>::pos_type IOStreamBuf<T>::current_position() const {
  pos_type pos = 0;

  for (const IOBuf* buf = head_; buf != gcur_; buf = buf->next())
    pos += buf->length();

  return pos + (this->gptr() - this->eback());
}

template <typename T>
typename IOStreamBuf<T>::pos_type
IOStreamBuf<T>::seekoff(off_type off,
                        std::ios_base::seekdir way,
                        std::ios_base::openmode which) {
  if ((which & std::ios_base::in) != std::ios_base::in)
    return badoff;

  if (way == std::ios_base::beg) {
    if (UNLIKELY(off < 0))
      return badoff;

    const IOBuf* buf = head_;

    auto remaining_offset = off;

    // TODO: Handle multi-byte T with sub-CharT buffer boundary
    while (remaining_offset > buf->length() / sizeof(T)) {
      remaining_offset -= buf->length() / sizeof(T);
      buf = buf->next();
      if (buf == head_)
        return badoff;
      }

    gcur_ = buf;
    csetg(gcur_->data(), gcur_->data() + remaining_offset * sizeof(T), gcur_->tail());

    return pos_type(off);
  }

  if (way == std::ios_base::end) {
    if (UNLIKELY(off > 0))
      return badoff;

    const IOBuf* buf = head_->prev();

    // Work with positive offset working back from the last tail()
    auto remaining_offset = 0 - off;

    while (remaining_offset > buf->length() / sizeof(T)) {
      remaining_offset -= buf->length() / sizeof(T);
      buf = buf->prev();
      if (buf == head_ && off > buf->length() / sizeof(T))
        return badoff;
    }

    gcur_ = buf;
    csetg(gcur_->data(), gcur_->tail() - remaining_offset * sizeof(T), gcur_->tail());

    return current_position();
  }

  if (way == std::ios_base::cur) {
    if (UNLIKELY(off == 0))
      return current_position();

    const IOBuf* buf = gcur_;

    auto remaining_offset = off;

    if (remaining_offset < 0) {
      // backwards; use as positive distance backward
      remaining_offset = 0 - remaining_offset;

      if (remaining_offset < this->gptr() - this->eback()) {
        // In the same IOBuf
        csetg(gcur_->data(), gcur_->data() + reinterpret_cast<size_t>(this->gptr() - this->eback() - remaining_offset * sizeof(T)), gcur_->tail());
        return current_position();
      }

      remaining_offset -= this->gptr() - this->eback();

      do {
        buf = buf->prev();

        if (buf == head_ && remaining_offset > buf->length() / sizeof(T))
          return badoff; // position precedes start of data

        remaining_offset -= buf->length() / sizeof(T);
      } while (remaining_offset > buf->length() / sizeof(T));

      gcur_ = buf;
      csetg(gcur_->data(), gcur_->tail() - remaining_offset * sizeof(T), gcur_->tail());
      return current_position();
    }

    assert(remaining_offset > 0);

    if (remaining_offset < this->egptr() - this->gptr()) {
      assert(reinterpret_cast<const uint8_t*>(this->egptr()) == gcur_->tail());
      csetg(gcur_->data(), reinterpret_cast<const uint8_t*>(this->gptr() + remaining_offset), gcur_->tail());
      return current_position();
    }

    remaining_offset -= this->egptr() - this->gptr();

    for (buf = buf->next();
         buf != head_;
         buf = buf->next()) {
      if (remaining_offset < buf->length() / sizeof(T)) {
        gcur_ = buf;
        csetg(gcur_->data(), gcur_->data() + remaining_offset * sizeof(T), gcur_->tail());
        return current_position();
      }

      remaining_offset -= buf->length() / sizeof(T);
    }

    return badoff;
  }

  return badoff;
}

template <typename T>
typename IOStreamBuf<T>::pos_type
IOStreamBuf<T>::seekpos(pos_type pos, std::ios_base::openmode which) {
  return seekoff(off_type(pos), std::ios_base::beg, which);
}

template <typename T>
std::streamsize IOStreamBuf<T>::showmanyc() {
  std::streamsize s = this->egptr() - this->gptr();

  for (const IOBuf* buf = gcur_->next(); buf != head_; buf = buf->next())
    s += buf->length() / sizeof(T);

  return s;
}

template <typename T>
std::streamsize IOStreamBuf<T>::xsgetn(char_type* s, std::streamsize count) {
  if (UNLIKELY(count < 0))
    return 0;

  std::streamsize copied = 0;

  std::streamsize n = std::min(this->egptr() - this->gptr(), static_cast<off_type>(count));
  // XXX: Check for overflow (write a memcpy_safe or something)
  memcpy(s, this->gptr(), n * sizeof(T));
  count -= n;
  copied += n;

  for (const IOBuf* buf = gcur_->next(); buf != head_ && count > 0; buf = buf->next()) {
    n = std::min(static_cast<std::streamsize>(buf->length() / sizeof(T)), static_cast<off_type>(count));
    // XXX: Check for overflow (as above)
    memcpy(s + copied * sizeof(T), buf->data(), n * sizeof(T));
    count -= n;
    copied += n;
  }

  this->gbump(copied);

  return copied;
}

template <typename T>
void IOStreamBuf<T>::csetg(const uint8_t* gbeg, const uint8_t* gcurr, const uint8_t* gend) {
  return this->setg(reinterpret_cast<T*>(const_cast<uint8_t*>(gbeg)),
                    reinterpret_cast<T*>(const_cast<uint8_t*>(gcurr)),
                    reinterpret_cast<T*>(const_cast<uint8_t*>(gend)));
}

} // namespace

// vim: ts=2 sw=2 tw=80 et ai