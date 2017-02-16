#include <memory>
#include <streambuf>

#include "IOBuf.h"

namespace folly {

class IOStreamBuf : public std::basic_streambuf<uint8_t> {
 public:
  // Constructors & states to consider:
  // We own the IOBuf.
  // We don't own the IOBuf.
  // Mutable or readonly.
  // COW.
  // strstreambuf seems to have good indications on handling these.
  IOStreamBuf(const std::shared_ptr<folly::IOBuf>& iobuf);

  IOStreamBuf(const IOStreamBuf&) = default;
  IOStreamBuf& operator=(const IOStreamBuf&) = default;
  void swap(const IOStreamBuf&);

  virtual ~IOStreamBuf() override = default;

 protected:
  // positioning

  // probably should do this
  virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir,
          std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
  // probably should allow this too
  virtual pos_type seekpos(pos_type pos,
          std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;

  // get area
  showmanyc();
  virtual std::basic_streambuf::int_type underflow() override;
  //uflow(); // optional; base calls overflow() & increments gptr.
  //virtual std::streamsize xsgetn(char_type* s, std::streamsize count) override; // optional but worth implementing.

  virtual int_type pbackfail(int_type c = Traits::eof()) override;

#if 0
  // put area
  xsputn();
  overflow();
  pbase();
  pptr();
  epptr();
  pbump();
  setp();
#endif

  pos_type current_position() const;

 private:
  std::shared_ptr<folly::IOBuf> head_;
  folly::IOBuf *gcur_; // current get IOBuf
  //folly::IOBuf *pcur_; // current put IOBuf
};

}

// vim: ts=2 sw=2 et ai
