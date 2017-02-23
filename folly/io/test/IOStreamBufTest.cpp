#include <istream>
#include <memory>
#include <string>

#include <folly/io/IOBuf.h>
#include <folly/io/IOStreamBuf.h>

#include <folly/portability/GTest.h>

using folly::IOBuf;
using folly::IOStreamBuf;

static std::unique_ptr<IOBuf> sampledata() {
  auto hello = IOBuf::copyBuffer(std::string("hello "));
  auto world = IOBuf::copyBuffer(std::string("world"));

  hello->prependChain(std::move(world));
  return std::move(hello);
}

// Tests via an istream.
TEST(IOStreamBuf, IStream) {
  std::unique_ptr<IOBuf> buf = sampledata();

  // XXX: Type kludgery
  // IOStreamBuf type needs to match istream type, and everything
  // needs to be cast to/from IOBuf's uint8_t
  IOStreamBuf<char> streambuf(buf.get());
  std::istream in(&streambuf);

  std::string s;
  in >> s;
  EXPECT_EQ(s, "hello world");

  in.seekg(1);
  s.clear();

  in >> s;
  EXPECT_EQ(s, "ello world");
}

// TODO: Tests directly on the streambuf

// FUTURE: Test multi-byte template argument (wchar_t)

// vim: ts=2 sw=2 et tw=80
