#include <memory>
#include <streambuf>

#include <folly/io/IOBuf.h>

namespace folly {

template<typename T>
class IOStreamBuf : public std::basic_streambuf<T> {
  // Due to having to merge single-byte subsets of CharT across IOBuf boundaries,
  // prevent the use of IOStreamBuf on multi-byte types for now.
  static_assert(sizeof(T) == 1, "IOStreamBuf doesn't yet work with multi-byte types");

 public:
  /**
   * Construct IOStreamBuf using the provided IOBuf, which may be the head of a
   * chain.
   * The IOStreamBuf does not own the IOBuf nor extend the lifetime of it; you
   * must ensure that the IOBuf provided lasts at least as long as the
   * IOStreamBuf.
   */
  IOStreamBuf(const folly::IOBuf* head);

  IOStreamBuf(const IOStreamBuf&) = default;
  IOStreamBuf& operator=(const IOStreamBuf&) = default;
  void swap(IOStreamBuf<T>&);

  virtual ~IOStreamBuf() override = default;

  using char_type = typename std::basic_streambuf<T>::char_type;
  using int_type = typename std::basic_streambuf<T>::int_type;
  using off_type = typename std::basic_streambuf<T>::off_type;
  using pos_type = typename std::basic_streambuf<T>::pos_type;
  using traits_type = typename std::basic_streambuf<T>::traits_type;

 protected:
  // positioning
  virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir,
          std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
  virtual pos_type seekpos(pos_type pos,
          std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;

  // get area
  virtual std::streamsize showmanyc() override;
  virtual int_type underflow() override;
  virtual std::streamsize xsgetn(char_type* s, std::streamsize count) override;
  virtual int_type pbackfail(int_type c = traits_type::eof()) override;

  pos_type current_position() const;

 private:
  const folly::IOBuf* head_;
  const folly::IOBuf* gcur_; // current get IOBuf
};

}

// vim: ts=2 sw=2 et ai tw=80
