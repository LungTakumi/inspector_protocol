// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "binary_encoding.h"

#include <array>
#include <string>
#include "base/strings/stringprintf.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "json_parser.h"
#include "json_std_string_writer.h"
#include "linux_dev_platform.h"

using testing::ElementsAreArray;

namespace inspector_protocol {
TEST(EncodeDecodeUnsignedTest, Roundtrips23) {
  // This roundtrips the uint64_t value 23 through the pair of EncodeUnsigned /
  // DecodeUnsigned functions; this is interesting since 23 is encoded as
  // a single byte.
  std::vector<uint8_t> encoded;
  EncodeUnsigned(23, &encoded);
  // first three bits: major type = 0; remaining five bits: additional info =
  // value 23.
  EXPECT_THAT(encoded, ElementsAreArray(std::array<uint8_t, 1>{{23}}));

  // Now the reverse direction: decode the encoded empty string and store it
  // into |decoded|.
  uint64_t decoded = 0;  // Assign != 23 to ensure it's not 23 by accident.
  span<uint8_t> encoded_bytes(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_TRUE(DecodeUnsigned(&encoded_bytes, &decoded));
  EXPECT_EQ(uint64_t(23), decoded);
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUnsignedTest, RoundtripsUint8) {
  // This roundtrips the uint64_t value 42 through the pair of EncodeUnsigned /
  // EncodeUnsigned functions. This is different from Roundtrip0 because
  // 42 is encoded in an extra byte after the initial one.
  std::vector<uint8_t> encoded;
  EncodeUnsigned(42, &encoded);
  // first three bits: major type = 0;
  // remaining five bits: additional info = 24, indicating payload is uint8.
  EXPECT_THAT(encoded, ElementsAreArray(std::array<uint8_t, 2>{{24, 42}}));

  // Reverse direction.
  uint64_t decoded = 0;
  span<uint8_t> encoded_bytes(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_TRUE(DecodeUnsigned(&encoded_bytes, &decoded));
  EXPECT_EQ(uint64_t(42), decoded);
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUnsignedTest, RoundtripsUint16) {
  // 500 is encoded as a uint16 after the initial byte.
  std::vector<uint8_t> encoded;
  EncodeUnsigned(500, &encoded);
  EXPECT_EQ(size_t(3), encoded.size());  // 1 for initial byte, 2 for uint16.
  // first three bits: major type = 0;
  // remaining five bits: additional info = 25, indicating payload is uint16.
  EXPECT_EQ(25, encoded[0]);
  EXPECT_EQ(0x01, encoded[1]);
  EXPECT_EQ(0xf4, encoded[2]);

  // Reverse direction.
  uint64_t decoded;
  span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeUnsigned(&encoded_bytes, &decoded));
  EXPECT_EQ(uint64_t(500), decoded);
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUnsignedTest, RoundtripsUint32) {
  // 0xdeadbeef is encoded as a uint32 after the initial byte.
  std::vector<uint8_t> encoded;
  EncodeUnsigned(0xdeadbeef, &encoded);
  // 1 for initial byte, 4 for the uint32.
  // first three bits: major type = 0;
  // remaining five bits: additional info = 26, indicating payload is uint32.
  EXPECT_THAT(
      encoded,
      ElementsAreArray(std::array<uint8_t, 5>{{26, 0xde, 0xad, 0xbe, 0xef}}));

  // Reverse direction.
  uint64_t decoded;
  span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeUnsigned(&encoded_bytes, &decoded));
  EXPECT_EQ(uint64_t(0xdeadbeef), decoded);
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUnsignedTest, RoundtripsUint64) {
  // 0xaabbccddeeff0011 is encoded as a uint64 after the initial byte
  std::vector<uint8_t> encoded;
  EncodeUnsigned(0xaabbccddeeff0011, &encoded);
  // 1 for initial byte, 8 for the uint64.
  // first three bits: major type = 0;
  // remaining five bits: additional info = 27, indicating payload is uint64.
  EXPECT_THAT(encoded,
              ElementsAreArray(std::array<uint8_t, 9>{
                  {27, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11}}));

  // Reverse direction.
  uint64_t decoded;
  span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeUnsigned(&encoded_bytes, &decoded));
  EXPECT_EQ(0xaabbccddeeff0011, decoded);
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUnsignedTest, ErrorCases) {
  struct TestCase {
    std::vector<uint8_t> data;
    std::string msg;
  };
  std::vector<TestCase> tests{
      {TestCase{
           {24},
           "additional info = 24 would require 1 byte of payload (but it's 0)"},
       TestCase{{27, 0xaa, 0xbb, 0xcc},
                "additional info = 27 would require 8 bytes of payload (but "
                "it's 3)"},
       TestCase{{2 << 5}, "we require major type 0 (but it's 2)"},
       TestCase{{29}, "additional info = 29 isn't recognized"}}};
  for (const TestCase& test : tests) {
    SCOPED_TRACE(test.msg);
    uint64_t decoded = 0xdeadbeef;  // unlikely to be written by accident.
    span<uint8_t> encoded_bytes(&test.data[0], test.data.size());
    EXPECT_FALSE(DecodeUnsigned(&encoded_bytes, &decoded));
    EXPECT_EQ(0xdeadbeef, decoded);  // unmodified
    EXPECT_EQ(test.data.size(), size_t(encoded_bytes.size()));
  }
}

namespace internal {
// EncodeNegative / DecodeNegative are not exposed in the header, but we
// still want to test a roundtrip here.
void EncodeNegative(int64_t value, std::vector<uint8_t>* out);
bool DecodeNegative(span<uint8_t>* bytes, int64_t* value);

TEST(EncodeDecodeNegativeTest, RoundtripsMinus24) {
  // This roundtrips the int64_t value -24 through the pair of EncodeNegative /
  // DecodeNegative functions; this is interesting since -24 is encoded as
  // a single byte, and it tests the specific encoding (note how for unsigned
  // the single byte covers values up to 23).
  // Additional examples are covered in RoundtripsAdditionalExamples.
  std::vector<uint8_t> encoded;
  EncodeNegative(-24, &encoded);
  // first three bits: major type = 0; remaining five bits: additional info =
  // value 23.
  EXPECT_THAT(encoded, ElementsAreArray(std::array<uint8_t, 1>{{1 << 5 | 23}}));

  // Now the reverse direction: decode the encoded empty string and store it
  // into decoded.
  int64_t decoded = 0;  // Assign != 23 to ensure it's not 23 by accident.
  span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeNegative(&encoded_bytes, &decoded));
  EXPECT_EQ(-24, decoded);
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeNegativeTest, RoundtripsAdditionalExamples) {
  std::vector<int64_t> examples = {-1,
                                   -10,
                                   -24,
                                   -25,
                                   -300,
                                   -30000,
                                   -300 * 1000,
                                   -1000 * 1000,
                                   -1000 * 1000 * 1000,
                                   -5 * 1000 * 1000 * 1000,
                                   std::numeric_limits<int64_t>::min()};
  for (int64_t example : examples) {
    SCOPED_TRACE(base::StringPrintf("example %ld", example));
    std::vector<uint8_t> encoded;
    EncodeNegative(example, &encoded);
    int64_t decoded = 0;
    span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
    EXPECT_TRUE(DecodeNegative(&encoded_bytes, &decoded));
    EXPECT_EQ(example, decoded);
    EXPECT_TRUE(encoded_bytes.empty());
  }
}
}  // namespace internal

TEST(EncodeDecodeUTF16StringTest, RoundtripsEmpty) {
  // This roundtrips the empty utf16 string through the pair of EncodeUTF16 /
  // EncodeUTF16 functions.
  std::vector<uint8_t> encoded;
  EncodeUTF16String(span<uint16_t>(), &encoded);
  EXPECT_EQ(size_t(1), encoded.size());
  // first three bits: major type = 2; remaining five bits: additional info =
  // size 0.
  EXPECT_EQ(2 << 5, encoded[0]);

  // Now the reverse direction: decode the encoded empty string and store it
  // into decoded.
  std::vector<uint16_t> decoded;
  span<uint8_t> encoded_bytes = span<uint8_t>(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeUTF16String(&encoded_bytes, &decoded));
  EXPECT_TRUE(decoded.empty());
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUTF16StringTest, RoundtripsHelloWorld) {
  // This roundtrips the hello world message which is given here in utf16
  // characters. 0xd83c, 0xdf0e: UTF16 encoding for the "Earth Globe Americas"
  // character, 🌎.
  std::array<uint16_t, 10> msg{
      {'H', 'e', 'l', 'l', 'o', ',', ' ', 0xd83c, 0xdf0e, '.'}};
  std::vector<uint8_t> encoded;
  EncodeUTF16String(span<uint16_t>(msg.data(), msg.size()), &encoded);
  // This will be encoded as BYTE_STRING of length 20, so the 20 is encoded in
  // the additional info part of the initial byte. Payload is two bytes for each
  // UTF16 character.
  uint8_t initial_byte = /*major type=*/2 << 5 | /*additional info=*/20;
  std::array<uint8_t, 21> encoded_expected = {
      {initial_byte, 'H', 0,   'e', 0,    'l',  0,    'l',  0,   'o', 0,
       ',',          0,   ' ', 0,   0x3c, 0xd8, 0x0e, 0xdf, '.', 0}};
  EXPECT_THAT(encoded, ElementsAreArray(encoded_expected));

  // Now decode to complete the roundtrip.
  std::vector<uint16_t> decoded;
  span<uint8_t> encoded_bytes = span<uint8_t>(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeUTF16String(&encoded_bytes, &decoded));
  EXPECT_THAT(decoded, ElementsAreArray(msg));
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUTF16StringTest, Roundtrips500) {
  // We roundtrip a message that has 250 16 bit values. Each of these are just
  // set to their index. 250 is interesting because the cbor spec uses a
  // BYTE_STRING of length 500 for one of their examples of how to encode the
  // start of it (section 2.1) so it's easy for us to look at the first three
  // bytes closely.
  std::vector<uint16_t> two_fifty;
  for (uint16_t ii = 0; ii < 250; ++ii) two_fifty.push_back(ii);
  std::vector<uint8_t> encoded;
  EncodeUTF16String(span<uint16_t>(two_fifty.data(), two_fifty.size()),
                    &encoded);
  EXPECT_EQ(size_t(3 + 250 * 2), encoded.size());
  // Now check the first three bytes:
  // Major type: 2 (BYTE_STRING)
  // Additional information: 25, indicating size is represented by 2 bytes.
  // Bytes 1 and 2 encode 500 (0x01f4).
  EXPECT_EQ(2 << 5 | 25, encoded[0]);
  EXPECT_EQ(0x01, encoded[1]);
  EXPECT_EQ(0xf4, encoded[2]);

  // Now decode to complete the roundtrip.
  std::vector<uint16_t> decoded;
  span<uint8_t> encoded_bytes = span<uint8_t>(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeUTF16String(&encoded_bytes, &decoded));
  EXPECT_THAT(decoded, ElementsAreArray(two_fifty));
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUTF16StringTest, ErrorCases) {
  struct TestCase {
    std::vector<uint8_t> data;
    std::string msg;
  };
  std::vector<TestCase> tests{
      {TestCase{{0}, "we require major type 2 (but it's 0)"},
       TestCase{{2 << 5 | 1, 'a'},
                "length must be divisible by 2 (but it's 1)"},
       TestCase{{2 << 5 | 29}, "additional info = 29 isn't recognized"}}};
  for (const TestCase& test : tests) {
    SCOPED_TRACE(test.msg);
    std::vector<uint16_t> decoded;
    span<uint8_t> encoded_bytes(&test.data[0], test.data.size());
    EXPECT_FALSE(DecodeUTF16String(&encoded_bytes, &decoded));
    EXPECT_TRUE(decoded.empty());
    EXPECT_EQ(test.data.size(), size_t(encoded_bytes.size()));
  }
}

TEST(EncodeDecode8StringTest, RoundtripsHelloWorld) {
  // This roundtrips the hello world message which is given here in utf8
  // characters. 🌎 is a four byte utf8 character.
  std::string utf8_msg = "Hello, 🌎.";
  std::vector<uint8_t> msg(utf8_msg.begin(), utf8_msg.end());
  std::vector<uint8_t> encoded;
  EncodeUTF8String(span<uint8_t>(msg.data(), msg.size()), &encoded);
  // This will be encoded as BYTE_STRING of length 20, so the 20 is encoded in
  // the additional info part of the initial byte. Payload is two bytes for each
  // UTF16 character.
  uint8_t initial_byte = /*major type=*/3 << 5 | /*additional info=*/12;
  std::array<uint8_t, 13> encoded_expected = {{initial_byte, 'H', 'e', 'l', 'l',
                                               'o', ',', ' ', 0xF0, 0x9f, 0x8c,
                                               0x8e, '.'}};
  EXPECT_THAT(encoded, ElementsAreArray(encoded_expected));

  // Now decode to complete the roundtrip.
  std::vector<uint8_t> decoded;
  span<uint8_t> encoded_bytes = span<uint8_t>(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeUTF8String(&encoded_bytes, &decoded));
  EXPECT_THAT(decoded, ElementsAreArray(msg));
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeDoubleTest, RoundtripsWikipediaExample) {
  // https://en.wikipedia.org/wiki/Double-precision_floating-point_format
  // provides the example of a hex representation 3FD5 5555 5555 5555, which
  // approximates 1/3.

  std::vector<uint8_t> encoded;
  EncodeDouble(1.0 / 3, &encoded);
  // first three bits: major type = 7; remaining five bits: additional info =
  // value 27. This is followed by 8 bytes of payload (which match Wikipedia).
  EXPECT_THAT(
      encoded,
      ElementsAreArray(std::array<uint8_t, 9>{
          {7 << 5 | 27, 0x3f, 0xd5, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55}}));

  // Now the reverse direction: decode the encoded empty string and store it
  // into decoded.
  double decoded = 0;
  span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeDouble(&encoded_bytes, &decoded));
  EXPECT_THAT(decoded, testing::DoubleEq(1.0 / 3));
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeDoubleTest, RoundtripsAdditionalExamples) {
  std::vector<double> examples = {0.0,
                                  1.0,
                                  -1.0,
                                  3.1415,
                                  std::numeric_limits<double>::min(),
                                  std::numeric_limits<double>::max(),
                                  std::numeric_limits<double>::infinity(),
                                  std::numeric_limits<double>::quiet_NaN()};
  for (double example : examples) {
    SCOPED_TRACE(base::StringPrintf("example %lf", example));
    std::vector<uint8_t> encoded;
    EncodeDouble(example, &encoded);
    double decoded = 0;
    span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
    EXPECT_TRUE(DecodeDouble(&encoded_bytes, &decoded));
    if (std::isnan(example)) {
      EXPECT_TRUE(std::isnan(decoded));
    } else {
      EXPECT_THAT(decoded, testing::DoubleEq(example));
    }
    EXPECT_TRUE(encoded_bytes.empty());
  }
}

void EncodeSevenBitStringForTest(const std::string& key,
                                 std::vector<uint8_t>* out) {
  for (char c : key) assert(c > 0);
  EncodeUTF8String(
      span<uint8_t>(reinterpret_cast<const uint8_t*>(key.data()), key.size()),
      out);
}

TEST(JsonToCborEncoderTest, SevenBitStrings) {
  // When a string can be represented as 7 bit ascii, the encoder will use the
  // STRING (major Type 2) type, so the actual characters end up as bytes on the
  // wire.
  std::vector<uint8_t> encoded;
  Status status;
  std::unique_ptr<JsonParserHandler> encoder =
      NewJsonToBinaryEncoder(&encoded, &status);
  std::vector<uint16_t> utf16;
  utf16.push_back('f');
  utf16.push_back('o');
  utf16.push_back('o');
  encoder->HandleString(utf16);
  EXPECT_EQ(Error::OK, status.error);
  // Here we assert that indeed, seven bit strings are represented as
  // bytes on the wire, "foo" is just "foo".
  EXPECT_THAT(encoded,
              ElementsAreArray(std::array<uint8_t, 4>{
                  {/*major type 3*/ 3 << 5 | /*length*/ 3, 'f', 'o', 'o'}}));
}

TEST(JsonCborRoundtrip, EncodingDecoding) {
  // Hits all the cases except error in JsonParserHandler, first parsing
  // a JSON message into CBOR, then parsing it back from CBOR into JSON.
  std::string json =
      "{"
      "\"string\":\"Hello, \\ud83c\\udf0e.\","
      "\"double\":3.1415,"
      "\"int\":1,"
      "\"negative int\":-1,"
      "\"bool\":true,"
      "\"null\":null,"
      "\"array\":[1,2,3]"
      "}";
  std::vector<uint8_t> encoded;
  Status status;
  std::unique_ptr<JsonParserHandler> encoder =
      NewJsonToBinaryEncoder(&encoded, &status);
  span<uint8_t> ascii_in(reinterpret_cast<const uint8_t*>(json.data()),
                         json.size());
  parseJSONChars(GetLinuxDevPlatform(), ascii_in, encoder.get());
  std::vector<uint8_t> expected;
  expected.push_back(0xbf);  // indef length map start
  EncodeSevenBitStringForTest("string", &expected);
  // This is followed by the encoded string for "Hello, 🌎."
  // So, it's the same bytes that we tested above in
  // EncodeDecodeUTF16StringTest.RoundtripsHelloWorld.
  expected.push_back(/*major type=*/2 << 5 | /*additional info=*/20);
  for (uint8_t ch : std::array<uint8_t, 20>{
           {'H', 0, 'e', 0, 'l',  0,    'l',  0,    'o', 0,
            ',', 0, ' ', 0, 0x3c, 0xd8, 0x0e, 0xdf, '.', 0}})
    expected.push_back(ch);
  EncodeSevenBitStringForTest("double", &expected);
  EncodeDouble(3.1415, &expected);
  EncodeSevenBitStringForTest("int", &expected);
  EncodeUnsigned(1, &expected);
  EncodeSevenBitStringForTest("negative int", &expected);
  internal::EncodeNegative(-1, &expected);
  EncodeSevenBitStringForTest("bool", &expected);
  expected.push_back(7 << 5 | 21);  // RFC 7049 Section 2.3, Table 2: true
  EncodeSevenBitStringForTest("null", &expected);
  expected.push_back(7 << 5 | 22);  // RFC 7049 Section 2.3, Table 2: null
  EncodeSevenBitStringForTest("array", &expected);
  expected.push_back(0x9f);  // RFC 7049 Section 2.2.1, indef length array start
  expected.push_back(1);     // Three UNSIGNED values (easy since Major Type 0)
  expected.push_back(2);
  expected.push_back(3);
  expected.push_back(0xff);  // End indef length array
  expected.push_back(0xff);  // End indef length map
  EXPECT_TRUE(status.ok());
  EXPECT_THAT(encoded, ElementsAreArray(expected));

  // And now we roundtrip, decoding the message we just encoded.
  std::string decoded;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &decoded, &status);
  ParseBinary(span<uint8_t>(encoded.data(), encoded.size()), json_writer.get());
  EXPECT_EQ(Error::OK, status.error);
  EXPECT_EQ(json, decoded);
}

TEST(JsonCborRoundtrip, MoreRoundtripExamples) {
  std::vector<std::string> examples = {
      // Tests that after closing a nested objects, additional key/value pairs
      // are considered.
      "{\"foo\":{\"bar\":1},\"baz\":2}", "{\"foo\":[1,2,3],\"baz\":2}"};
  for (const std::string& json : examples) {
    SCOPED_TRACE(std::string("example: ") + json);
    std::vector<uint8_t> encoded;
    Status status;
    std::unique_ptr<JsonParserHandler> encoder =
        NewJsonToBinaryEncoder(&encoded, &status);
    span<uint8_t> ascii_in(reinterpret_cast<const uint8_t*>(json.data()),
                           json.size());
    parseJSONChars(GetLinuxDevPlatform(), ascii_in, encoder.get());
    std::string decoded;
    std::unique_ptr<JsonParserHandler> json_writer =
        NewJsonWriter(GetLinuxDevPlatform(), &decoded, &status);
    ParseBinary(span<uint8_t>(encoded.data(), encoded.size()),
                json_writer.get());
    EXPECT_EQ(Error::OK, status.error);
    EXPECT_EQ(json, decoded);
  }
}

TEST(ParseBinaryTest, ParseEmptyBinaryMessage) {
  // Just an indefinite length map that's empty (0xff = stop byte).
  std::vector<uint8_t> in = {0xbf, 0xff};
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(span<uint8_t>(in.data(), in.size()), json_writer.get());
  EXPECT_EQ(Error::OK, status.error);
  EXPECT_EQ("{}", out);
}

TEST(ParseBinaryTest, ParseBinaryHelloWorld) {
  std::vector<uint8_t> bytes;

  bytes.push_back(0xbf);                    // start indef length map.
  EncodeSevenBitStringForTest("msg", &bytes);  // key: msg
  // Now write the value, the familiar "Hello, 🌎." where the globe is expressed
  // as two utf16 chars.
  bytes.push_back(/*major type=*/2 << 5 | /*additional info=*/20);
  for (uint8_t ch : std::array<uint8_t, 20>{
           {'H', 0, 'e', 0, 'l',  0,    'l',  0,    'o', 0,
            ',', 0, ' ', 0, 0x3c, 0xd8, 0x0e, 0xdf, '.', 0}})
    bytes.push_back(ch);
  bytes.push_back(0xff);  // stop byte

  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::OK, status.error);
  EXPECT_EQ("{\"msg\":\"Hello, \\ud83c\\udf0e.\"}", out);
}

TEST(ParseBinaryTest, NoInputError) {
  std::vector<uint8_t> in = {};
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(span<uint8_t>(in.data(), in.size()), json_writer.get());
  EXPECT_EQ(Error::BINARY_ENCODING_NO_INPUT, status.error);
  EXPECT_EQ("", out);
}

TEST(ParseBinaryTest, InvalidStartByteError) {
  // Here we test that some actual json, which usually starts with {,
  // is not considered a binary message. Binary messages must start with
  // 0xbf, the indefinite length map start byte.
  std::string json = "{\"msg\": \"Hello, world.\"}";
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      json_writer.get());
  EXPECT_EQ(Error::BINARY_ENCODING_INVALID_START_BYTE, status.error);
  EXPECT_EQ("", out);
}

TEST(ParseBinaryTest, UnexpectedEofExpectedValueError) {
  std::vector<uint8_t> bytes = {0xbf};      // The byte for starting a map.
  EncodeSevenBitStringForTest("key", &bytes);  // A key; so value would be next.
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::BINARY_ENCODING_UNEXPECTED_EOF_EXPECTED_VALUE, status.error);
  EXPECT_EQ(int64_t(bytes.size()), status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseBinaryTest, UnexpectedEofInArrayError) {
  std::vector<uint8_t> bytes = {0xbf};        // The byte for starting a map.
  EncodeSevenBitStringForTest("array",
                              &bytes);  // A key; so value would be next.
  bytes.push_back(0x9f);  // byte for indefinite length array start.
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::BINARY_ENCODING_UNEXPECTED_EOF_IN_ARRAY, status.error);
  EXPECT_EQ(int64_t(bytes.size()), status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseBinaryTest, UnexpectedEofInMapError) {
  std::vector<uint8_t> bytes = {0xbf};  // The byte for starting a map.
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::BINARY_ENCODING_UNEXPECTED_EOF_IN_MAP, status.error);
  EXPECT_EQ(1, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseBinaryTest, InvalidMapKeyError) {
  // The byte for starting a map, followed by a byte representing null.
  // null is not a valid map key.
  std::vector<uint8_t> bytes = {0xbf, 7 << 5 | 22};
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::BINARY_ENCODING_INVALID_MAP_KEY, status.error);
  EXPECT_EQ(1, status.pos);
  EXPECT_EQ("", out);
}

std::vector<uint8_t> MakeNestedBinary(int depth) {
  std::vector<uint8_t> bytes;
  for (int ii = 0; ii < depth; ++ii) {
    bytes.push_back(0xbf);  // indef length map start
    EncodeSevenBitStringForTest("key", &bytes);
  }
  EncodeSevenBitStringForTest("innermost_value", &bytes);
  for (int ii = 0; ii < depth; ++ii)
    bytes.push_back(0xff);  // stop byte, finishes map.
  return bytes;
}

TEST(ParseBinaryTest, StackLimitExceededError) {
  {  // Depth 3: no stack limit exceeded error and is easy to inspect.
    std::vector<uint8_t> bytes = MakeNestedBinary(3);
    std::string out;
    Status status;
    std::unique_ptr<JsonParserHandler> json_writer =
        NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
    ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
    EXPECT_EQ(Error::OK, status.error);
    EXPECT_EQ(Status::npos(), status.pos);
    EXPECT_EQ("{\"key\":{\"key\":{\"key\":\"innermost_value\"}}}", out);
  }
  {  // Depth 1000: no stack limit exceeded.
    std::vector<uint8_t> bytes = MakeNestedBinary(1000);
    std::string out;
    Status status;
    std::unique_ptr<JsonParserHandler> json_writer =
        NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
    ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
    EXPECT_EQ(Error::OK, status.error);
    EXPECT_EQ(Status::npos(), status.pos);
  }

  // We just want to know the length of one opening map so we can compute
  // where the error is encountered.
  std::vector<uint8_t> opening_segment = {0xbf};
  EncodeSevenBitStringForTest("key", &opening_segment);

  {  // Depth 1001: limit exceeded.
    std::vector<uint8_t> bytes = MakeNestedBinary(1001);
    std::string out;
    Status status;
    std::unique_ptr<JsonParserHandler> json_writer =
        NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
    ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
    EXPECT_EQ(Error::BINARY_ENCODING_STACK_LIMIT_EXCEEDED, status.error);
    EXPECT_EQ(int64_t(opening_segment.size()) * 1001, status.pos);
  }
  {  // Depth 1200: still limit exceeded, and at the same pos as for 1001
    std::vector<uint8_t> bytes = MakeNestedBinary(1200);
    std::string out;
    Status status;
    std::unique_ptr<JsonParserHandler> json_writer =
        NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
    ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
    EXPECT_EQ(Error::BINARY_ENCODING_STACK_LIMIT_EXCEEDED, status.error);
    EXPECT_EQ(int64_t(opening_segment.size()) * 1001, status.pos);
  }
}

TEST(ParseBinaryTest, UnsupportedValueError) {
  std::vector<uint8_t> bytes = {0xbf};  // start indef length map.
  EncodeSevenBitStringForTest("key", &bytes);
  int64_t error_pos = bytes.size();
  bytes.push_back(6 << 5 | 5);  // tags aren't supported yet.
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::BINARY_ENCODING_UNSUPPORTED_VALUE, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseBinaryTest, InvalidString16Error) {
  std::vector<uint8_t> bytes = {0xbf};  // start indef length map.
  EncodeSevenBitStringForTest("key", &bytes);
  int64_t error_pos = bytes.size();
  // a BYTE_STRING of length 5 as value; since we interpret these as string16,
  // it's going to be invalid as each character would need two bytes, but
  // 5 isn't divisible by 2.
  bytes.push_back(2 << 5 | 5);
  for (int ii = 0; ii < 5; ++ii) bytes.push_back(' ');
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::BINARY_ENCODING_INVALID_STRING16, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseBinaryTest, InvalidString8Error) {
  std::vector<uint8_t> bytes = {0xbf};  // start indef length map.
  EncodeSevenBitStringForTest("key", &bytes);
  int64_t error_pos = bytes.size();
  // a STRING of length 5 as value, but we're at the end of the bytes array
  // so it can't be decoded successfully.
  bytes.push_back(3 << 5 | 5);
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::BINARY_ENCODING_INVALID_STRING8, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseBinaryTest, String8MustBe7BitError) {
  std::vector<uint8_t> bytes = {0xbf};  // start indef length map.
  EncodeSevenBitStringForTest("key", &bytes);
  int64_t error_pos = bytes.size();
  // a STRING of length 5 as value, with a payload that has bytes outside
  // 7 bit (> 0x7f).
  bytes.push_back(3 << 5 | 5);
  for (int ii = 0; ii < 5; ++ii) bytes.push_back(0xf0);
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::BINARY_ENCODING_STRING8_MUST_BE_7BIT, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseBinaryTest, InvalidDoubleError) {
  std::vector<uint8_t> bytes = {0xbf};  // start indef length map.
  EncodeSevenBitStringForTest("key", &bytes);
  int64_t error_pos = bytes.size();
  bytes.push_back(7 << 5 | 27);  // initial byte for double
  // Just two garbage bytes, not enough to represent an actual double.
  bytes.push_back(0x31);
  bytes.push_back(0x23);
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::BINARY_ENCODING_INVALID_DOUBLE, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseBinaryTest, InvalidSignedError) {
  std::vector<uint8_t> bytes = {0xbf};  // start indef length map.
  EncodeSevenBitStringForTest("key", &bytes);
  int64_t error_pos = bytes.size();
  // uint64_t max is a perfectly fine value to encode as CBOR unsigned,
  // but we don't support this since we only cover the int32_t range.
  EncodeUnsigned(std::numeric_limits<uint64_t>::max(), &bytes);
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> json_writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  ParseBinary(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::BINARY_ENCODING_INVALID_SIGNED, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}
}  // namespace inspector_protocol
