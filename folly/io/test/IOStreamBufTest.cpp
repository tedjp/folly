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

// Convenience function:
// Create a basic_string<T> from a basic_string<char>
template <typename T>
static std::basic_string<T> typedString(const std::string& in) {
  // Simply cast the string instead of widening since we only support
  // 1-octet types for now.
  static_assert(sizeof(T) == 1,
      "Casting without widening only works when sizeof(T) == 1");

  std::basic_string<T> out;
  out.append(reinterpret_cast<const T*>(in.data()), in.size());
  return out;
}

template <typename T>
class IOStreamBufTest : public ::testing::Test {
 public:
  static const T newline = static_cast<T>('\n');
};

typedef ::testing::Types<char, unsigned char, uint8_t, signed char> IOStreamBufTestTypes;

TYPED_TEST_CASE(IOStreamBufTest, IOStreamBufTestTypes);

// Tests via an istream.
TYPED_TEST(IOStreamBufTest, IStream) {
  auto bufp = sampledata();

  IOStreamBuf<TypeParam> streambuf(bufp.get());
  std::basic_istream<TypeParam> in(&streambuf);

  std::basic_string<TypeParam> s;
  std::getline(in, s, TestFixture::newline);
  EXPECT_EQ(s, typedString<TypeParam>("hello world"));
  EXPECT_TRUE(in.eof());

  in.seekg(1);

  TypeParam c;
  in.get(c);
  EXPECT_EQ(c, 'e');
  in.get(c);
  EXPECT_EQ(c, 'l');
  ASSERT_EQ(in.tellg(), 3);

  std::getline(in, s, TestFixture::newline);
  EXPECT_EQ(s, typedString<TypeParam>("lo world"));

  in.seekg(-2, std::ios_base::end);
  EXPECT_FALSE(in.eof());

  std::getline(in, s, TestFixture::newline);
  EXPECT_EQ(s, typedString<TypeParam>("ld"));

  // Seek from start
  in.seekg( 7, std::ios_base::beg);
  TypeParam raw[] = "\xfb\xfb";
  in.get(raw, sizeof(raw), TestFixture::newline);
  EXPECT_EQ(raw[0], static_cast<TypeParam>('o'));
  EXPECT_EQ(raw[1], static_cast<TypeParam>('r'));
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

  std::getline(in, s, TestFixture::newline);
  EXPECT_EQ(s, typedString<TypeParam>("o world"));

  EXPECT_TRUE(in.eof());
  ASSERT_FALSE(in.bad());

  // Test xsgetn (via basic_istream::read)
  in.seekg(0);
  ASSERT_EQ(in.tellg(), 0);

  TypeParam cdata[sizeof("zzhello worldzz")];
  in.read(cdata, sizeof(cdata));
  EXPECT_TRUE(in.eof());
  EXPECT_TRUE(in.fail()); // short read = fail
  EXPECT_FALSE(in.bad());
  in.clear(); // clear failbit
  std::basic_string<TypeParam> check(cdata, cdata + in.gcount());
  EXPECT_EQ(check, typedString<TypeParam>("hello world"));

  in.seekg(1);
  ASSERT_EQ(in.tellg(), 1);
  std::memset(cdata, '\xfb', sizeof(cdata));
  in.read(cdata, 6);

  EXPECT_EQ(in.gcount(), 6);
  check = std::basic_string<TypeParam>(cdata, cdata + 6);
  EXPECT_EQ(check, typedString<TypeParam>("ello w"));

  // putback
  in.putback(static_cast<TypeParam>('w'));
  ASSERT_TRUE(in.good());
  in.putback(static_cast<TypeParam>(' ')); // continue into the previous IOBuf
  ASSERT_TRUE(in.good());
  in.putback(static_cast<TypeParam>('z')); // non-matching putback
  EXPECT_FALSE(in.good());
}

// vim: ts=2 sw=2 et tw=80
