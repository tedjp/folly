#include <istream>
#include <memory>
#include <string>
#include <vector>

#include <folly/io/IOBuf.h>

#include <folly/portability/GTest.h>

using folly::IOBuf;
using folly::IOStreamBuf;

static std::unique_ptr<IOBuf> sampledata() {
  auto hello = IOBuf::copyBuffer(string("hello "));
  auto world = IOBuf::copyBuffer(string("world"));

  hello.prependChain(std::move(world));
  return std::move(hello);
}

// Tests via an istream.
TEST(IOStreamBuf, IStream) {
  std::vector v;
  v.resize(11);

  std::unique_ptr<IOBuf> buf = sampledata();

  IOStreamBuf streambuf(buf);
  std::istream in(streambuf);

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
