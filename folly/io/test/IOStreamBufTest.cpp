#include <cstring>
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
  IOStreamBuf<uint8_t> streambuf(buf.get());
  std::basic_istream<uint8_t> in(&streambuf);

  std::basic_string<uint8_t> s;
  std::getline(in, s);
  EXPECT_EQ(s, "hello world");
  EXPECT_TRUE(in.eof());

  in.seekg(1);

  uint8_t c;
  in.get(c);
  EXPECT_EQ(c, 'e');
  in.get(c);
  EXPECT_EQ(c, 'l');
  ASSERT_EQ(in.tellg(), 3);

  std::getline(in, s);
  EXPECT_EQ(s, "lo world");

  in.seekg(-2, std::ios_base::end);
  EXPECT_FALSE(in.eof());

  std::getline(in, s);
  EXPECT_EQ(s, "ld");

  // Seek from start
  in.seekg( 7, std::ios_base::beg);
  uint8_t uint8_ts[] = "\xfb\xfb";
  in.get(uint8_ts, sizeof(uint8_ts));
  EXPECT_EQ(uint8_ts[0], 'o');
  EXPECT_EQ(uint8_ts[1], 'r');
  EXPECT_FALSE(in.eof());
  EXPECT_FALSE(in.fail());

  // Seek from end
  in.seekg(-2, std::ios_base::end);
  EXPECT_EQ(in.tellg(), 9);
  in.seekg(-9, std::ios_base::end);
  EXPECT_EQ(in.tellg(), 2);

  // Seek from cur
  in.seekg( 0, std::ios_base::end);
  in.seekg(-9, std::ios_base::cur);
  in.seekg( 2, std::ios_base::cur);
  ASSERT_EQ(in.tellg(), 4);

  std::getline(in, s);
  EXPECT_EQ(s, "o world");

  EXPECT_TRUE(in.eof());
  ASSERT_FALSE(in.bad());

  // Test xsgetn (via basic_istream::read)
  in.seekg(0);
  ASSERT_EQ(in.tellg(), 0);

  uint8_t cdata[sizeof("zzhello worldzz")];
  in.read(cdata, sizeof(cdata));
  EXPECT_TRUE(in.eof());
  EXPECT_TRUE(in.fail()); // short read = fail
  EXPECT_FALSE(in.bad());
  in.clear(); // clear failbit
  std::string check(cdata, cdata + in.gcount());
  EXPECT_EQ(check, "hello world");

  in.seekg(1);
  ASSERT_EQ(in.tellg(), 1);
  std::memset(cdata, '\xfb', sizeof(cdata));
  in.read(cdata, 6);

  EXPECT_EQ(in.gcount(), 6);
  check = std::string(cdata, cdata + 6);
  EXPECT_EQ(check, "ello w");

  // putback
  in.putback('w');
  ASSERT_TRUE(in.good());
  in.putback(' '); // continue into the previous IOBuf
  ASSERT_TRUE(in.good());
  in.putback('z'); // non-matching putback
  EXPECT_FALSE(in.good());
}

// TODO: Tests directly on the streambuf

// FUTURE: Test multi-byte template argument (wchar_t)

// vim: ts=2 sw=2 et tw=80
