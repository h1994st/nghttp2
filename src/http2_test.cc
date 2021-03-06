/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2013 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "http2_test.h"

#include <cassert>
#include <cstring>
#include <iostream>

#include <CUnit/CUnit.h>

#include "http-parser/http_parser.h"

#include "http2.h"
#include "util.h"

using namespace nghttp2;

#define MAKE_NV(K, V)                                                          \
  {                                                                            \
    (uint8_t *) K, (uint8_t *)V, sizeof(K) - 1, sizeof(V) - 1,                 \
        NGHTTP2_NV_FLAG_NONE                                                   \
  }

namespace shrpx {

namespace {
void check_nv(const HeaderRef &a, const nghttp2_nv *b) {
  CU_ASSERT(a.name.size() == b->namelen);
  CU_ASSERT(a.value.size() == b->valuelen);
  CU_ASSERT(memcmp(a.name.c_str(), b->name, b->namelen) == 0);
  CU_ASSERT(memcmp(a.value.c_str(), b->value, b->valuelen) == 0);
}
} // namespace

void test_http2_add_header(void) {
  auto nva = Headers();

  http2::add_header(nva, (const uint8_t *)"alpha", 5, (const uint8_t *)"123", 3,
                    false, -1);
  CU_ASSERT(Headers::value_type("alpha", "123") == nva[0]);
  CU_ASSERT(!nva[0].no_index);

  nva.clear();

  http2::add_header(nva, (const uint8_t *)"alpha", 5, (const uint8_t *)"", 0,
                    true, -1);
  CU_ASSERT(Headers::value_type("alpha", "") == nva[0]);
  CU_ASSERT(nva[0].no_index);

  nva.clear();

  http2::add_header(nva, (const uint8_t *)"a", 1, (const uint8_t *)" b", 2,
                    false, -1);
  CU_ASSERT(Headers::value_type("a", "b") == nva[0]);

  nva.clear();

  http2::add_header(nva, (const uint8_t *)"a", 1, (const uint8_t *)"b ", 2,
                    false, -1);
  CU_ASSERT(Headers::value_type("a", "b") == nva[0]);

  nva.clear();

  http2::add_header(nva, (const uint8_t *)"a", 1, (const uint8_t *)"  b  ", 5,
                    false, -1);
  CU_ASSERT(Headers::value_type("a", "b") == nva[0]);

  nva.clear();

  http2::add_header(nva, (const uint8_t *)"a", 1, (const uint8_t *)"  bravo  ",
                    9, false, -1);
  CU_ASSERT(Headers::value_type("a", "bravo") == nva[0]);

  nva.clear();

  http2::add_header(nva, (const uint8_t *)"a", 1, (const uint8_t *)"    ", 4,
                    false, -1);
  CU_ASSERT(Headers::value_type("a", "") == nva[0]);

  nva.clear();

  http2::add_header(nva, (const uint8_t *)"te", 2, (const uint8_t *)"trailers",
                    8, false, http2::HD_TE);
  CU_ASSERT(http2::HD_TE == nva[0].token);
}

void test_http2_get_header(void) {
  auto nva = Headers{{"alpha", "1"},
                     {"bravo", "2"},
                     {"bravo", "3"},
                     {"charlie", "4"},
                     {"delta", "5"},
                     {"echo", "6"},
                     {"content-length", "7"}};
  const Headers::value_type *rv;
  rv = http2::get_header(nva, "delta");
  CU_ASSERT(rv != nullptr);
  CU_ASSERT("delta" == rv->name);

  rv = http2::get_header(nva, "bravo");
  CU_ASSERT(rv != nullptr);
  CU_ASSERT("bravo" == rv->name);

  rv = http2::get_header(nva, "foxtrot");
  CU_ASSERT(rv == nullptr);

  http2::HeaderIndex hdidx;
  http2::init_hdidx(hdidx);
  hdidx[http2::HD_CONTENT_LENGTH] = 6;
  rv = http2::get_header(hdidx, http2::HD_CONTENT_LENGTH, nva);
  CU_ASSERT("content-length" == rv->name);
}

namespace {
auto headers = HeaderRefs{
    {StringRef::from_lit("alpha"), StringRef::from_lit("0"), true},
    {StringRef::from_lit("bravo"), StringRef::from_lit("1")},
    {StringRef::from_lit("connection"), StringRef::from_lit("2"), false,
     http2::HD_CONNECTION},
    {StringRef::from_lit("connection"), StringRef::from_lit("3"), false,
     http2::HD_CONNECTION},
    {StringRef::from_lit("delta"), StringRef::from_lit("4")},
    {StringRef::from_lit("expect"), StringRef::from_lit("5")},
    {StringRef::from_lit("foxtrot"), StringRef::from_lit("6")},
    {StringRef::from_lit("tango"), StringRef::from_lit("7")},
    {StringRef::from_lit("te"), StringRef::from_lit("8"), false, http2::HD_TE},
    {StringRef::from_lit("te"), StringRef::from_lit("9"), false, http2::HD_TE},
    {StringRef::from_lit("x-forwarded-proto"), StringRef::from_lit("10"), false,
     http2::HD_X_FORWARDED_FOR},
    {StringRef::from_lit("x-forwarded-proto"), StringRef::from_lit("11"), false,
     http2::HD_X_FORWARDED_FOR},
    {StringRef::from_lit("zulu"), StringRef::from_lit("12")}};
} // namespace

void test_http2_copy_headers_to_nva(void) {
  auto ans = std::vector<int>{0, 1, 4, 5, 6, 7, 12};
  std::vector<nghttp2_nv> nva;

  http2::copy_headers_to_nva_nocopy(nva, headers);
  CU_ASSERT(7 == nva.size());
  for (size_t i = 0; i < ans.size(); ++i) {
    check_nv(headers[ans[i]], &nva[i]);

    if (ans[i] == 0) {
      CU_ASSERT((NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE |
                 NGHTTP2_NV_FLAG_NO_INDEX) == nva[i].flags);
    } else {
      CU_ASSERT((NGHTTP2_NV_FLAG_NO_COPY_NAME |
                 NGHTTP2_NV_FLAG_NO_COPY_VALUE) == nva[i].flags);
    }
  }

  nva.clear();
  http2::copy_headers_to_nva(nva, headers);
  CU_ASSERT(7 == nva.size());
  for (size_t i = 0; i < ans.size(); ++i) {
    check_nv(headers[ans[i]], &nva[i]);

    if (ans[i] == 0) {
      CU_ASSERT(nva[i].flags & NGHTTP2_NV_FLAG_NO_INDEX);
    } else {
      CU_ASSERT(NGHTTP2_NV_FLAG_NONE == nva[i].flags);
    }
  }
}

void test_http2_build_http1_headers_from_headers(void) {
  MemchunkPool pool;
  DefaultMemchunks buf(&pool);
  http2::build_http1_headers_from_headers(&buf, headers);
  auto hdrs = std::string(buf.head->pos, buf.head->last);
  CU_ASSERT("Alpha: 0\r\n"
            "Bravo: 1\r\n"
            "Delta: 4\r\n"
            "Expect: 5\r\n"
            "Foxtrot: 6\r\n"
            "Tango: 7\r\n"
            "Te: 8\r\n"
            "Te: 9\r\n"
            "Zulu: 12\r\n" == hdrs);
}

void test_http2_lws(void) {
  CU_ASSERT(!http2::lws("alpha"));
  CU_ASSERT(http2::lws(" "));
  CU_ASSERT(http2::lws(""));
}

namespace {
void check_rewrite_location_uri(const std::string &want, const std::string &uri,
                                const std::string &match_host,
                                const std::string &req_authority,
                                const std::string &upstream_scheme) {
  BlockAllocator balloc(4096, 4096);
  http_parser_url u{};
  CU_ASSERT(0 == http_parser_parse_url(uri.c_str(), uri.size(), 0, &u));
  auto got = http2::rewrite_location_uri(
      balloc, StringRef{uri}, u, StringRef{match_host},
      StringRef{req_authority}, StringRef{upstream_scheme});
  CU_ASSERT(want == got);
}
} // namespace

void test_http2_rewrite_location_uri(void) {
  check_rewrite_location_uri("https://localhost:3000/alpha?bravo#charlie",
                             "http://localhost:3001/alpha?bravo#charlie",
                             "localhost:3001", "localhost:3000", "https");
  check_rewrite_location_uri("https://localhost/", "http://localhost:3001/",
                             "localhost", "localhost", "https");
  check_rewrite_location_uri("http://localhost/", "http://localhost:3001/",
                             "localhost", "localhost", "http");
  check_rewrite_location_uri("http://localhost:443/", "http://localhost:3001/",
                             "localhost", "localhost:443", "http");
  check_rewrite_location_uri("https://localhost:80/", "http://localhost:3001/",
                             "localhost", "localhost:80", "https");
  check_rewrite_location_uri("", "http://localhost:3001/", "127.0.0.1",
                             "127.0.0.1", "https");
  check_rewrite_location_uri("https://localhost:3000/",
                             "http://localhost:3001/", "localhost",
                             "localhost:3000", "https");
  check_rewrite_location_uri("https://localhost:3000/", "http://localhost/",
                             "localhost", "localhost:3000", "https");

  // match_host != req_authority
  check_rewrite_location_uri("https://example.org", "http://127.0.0.1:8080",
                             "127.0.0.1", "example.org", "https");
  check_rewrite_location_uri("", "http://example.org", "127.0.0.1",
                             "example.org", "https");
}

void test_http2_parse_http_status_code(void) {
  CU_ASSERT(200 == http2::parse_http_status_code(StringRef::from_lit("200")));
  CU_ASSERT(102 == http2::parse_http_status_code(StringRef::from_lit("102")));
  CU_ASSERT(-1 == http2::parse_http_status_code(StringRef::from_lit("099")));
  CU_ASSERT(-1 == http2::parse_http_status_code(StringRef::from_lit("99")));
  CU_ASSERT(-1 == http2::parse_http_status_code(StringRef::from_lit("-1")));
  CU_ASSERT(-1 == http2::parse_http_status_code(StringRef::from_lit("20a")));
  CU_ASSERT(-1 == http2::parse_http_status_code(StringRef{}));
}

void test_http2_index_header(void) {
  http2::HeaderIndex hdidx;
  http2::init_hdidx(hdidx);

  http2::index_header(hdidx, http2::HD__AUTHORITY, 0);
  http2::index_header(hdidx, -1, 1);

  CU_ASSERT(0 == hdidx[http2::HD__AUTHORITY]);
}

void test_http2_lookup_token(void) {
  CU_ASSERT(http2::HD__AUTHORITY == http2::lookup_token(":authority"));
  CU_ASSERT(-1 == http2::lookup_token(":authorit"));
  CU_ASSERT(-1 == http2::lookup_token(":Authority"));
  CU_ASSERT(http2::HD_EXPECT == http2::lookup_token("expect"));
}

void test_http2_parse_link_header(void) {
  {
    // only URI appears; we don't extract URI unless it bears rel=preload
    constexpr char s[] = "<url>";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // URI url should be extracted
    constexpr char s[] = "<url>; rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // With extra link-param.  URI url should be extracted
    constexpr char s[] = "<url>; rel=preload; as=file";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // With extra link-param.  URI url should be extracted
    constexpr char s[] = "<url>; as=file; rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // With extra link-param and quote-string.  URI url should be
    // extracted
    constexpr char s[] = R"(<url>; rel=preload; title="foo,bar")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // With extra link-param and quote-string.  URI url should be
    // extracted
    constexpr char s[] = R"(<url>; title="foo,bar"; rel=preload)";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // ',' after quote-string
    constexpr char s[] = R"(<url>; title="foo,bar", <url2>; rel=preload)";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url2" == res[0].uri);
    CU_ASSERT(&s[25] == &res[0].uri[0]);
  }
  {
    // Only first URI should be extracted.
    constexpr char s[] = "<url>; rel=preload, <url2>";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // Both have rel=preload, so both urls should be extracted
    constexpr char s[] = "<url>; rel=preload, <url2>; rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(2 == res.size());
    CU_ASSERT("url" == res[0].uri);
    CU_ASSERT("url2" == res[1].uri);
  }
  {
    // Second URI uri should be extracted.
    constexpr char s[] = "<url>, <url2>;rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url2" == res[0].uri);
  }
  {
    // Error if input ends with ';'
    constexpr char s[] = "<url>;rel=preload;";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // Error if link header ends with ';'
    constexpr char s[] = "<url>;rel=preload;, <url>";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // OK if input ends with ','
    constexpr char s[] = "<url>;rel=preload,";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // Multiple repeated ','s between fields is OK
    constexpr char s[] = "<url>,,,<url2>;rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url2" == res[0].uri);
  }
  {
    // Error if url is not enclosed by <>
    constexpr char s[] = "url>;rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // Error if url is not enclosed by <>
    constexpr char s[] = "<url;rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // Empty parameter value is not allowed
    constexpr char s[] = "<url>;rel=preload; as=";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // Empty parameter value is not allowed
    constexpr char s[] = "<url>;as=;rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // Empty parameter value is not allowed
    constexpr char s[] = "<url>;as=, <url>;rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // Empty parameter name is not allowed
    constexpr char s[] = "<url>; =file; rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // Without whitespaces
    constexpr char s[] = "<url>;as=file;rel=preload,<url2>;rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(2 == res.size());
    CU_ASSERT("url" == res[0].uri);
    CU_ASSERT("url2" == res[1].uri);
  }
  {
    // link-extension may have no value
    constexpr char s[] = "<url>; as; rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // ext-name-star
    constexpr char s[] = "<url>; foo*=bar; rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // '*' is not allowed expect for trailing one
    constexpr char s[] = "<url>; *=bar; rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // '*' is not allowed expect for trailing one
    constexpr char s[] = "<url>; foo*bar=buzz; rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // ext-name-star must be followed by '='
    constexpr char s[] = "<url>; foo*; rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // '>' is not followed by ';'
    constexpr char s[] = "<url> rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // Starting with whitespace is no problem.
    constexpr char s[] = "  <url>; rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // preload is a prefix of bogus rel parameter value
    constexpr char s[] = "<url>; rel=preloadx";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // preload in relation-types list
    constexpr char s[] = R"(<url>; rel="preload")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // preload in relation-types list followed by another parameter
    constexpr char s[] = R"(<url>; rel="preload foo")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // preload in relation-types list following another parameter
    constexpr char s[] = R"(<url>; rel="foo preload")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // preload in relation-types list between other parameters
    constexpr char s[] = R"(<url>; rel="foo preload bar")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // preload in relation-types list between other parameters
    constexpr char s[] = R"(<url>; rel="foo   preload   bar")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // no preload in relation-types list
    constexpr char s[] = R"(<url>; rel="foo")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // no preload in relation-types list, multiple unrelated elements.
    constexpr char s[] = R"(<url>; rel="foo bar")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // preload in relation-types list, followed by another link-value.
    constexpr char s[] = R"(<url>; rel="preload", <url2>)";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // preload in relation-types list, following another link-value.
    constexpr char s[] = R"(<url>, <url2>; rel="preload")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url2" == res[0].uri);
  }
  {
    // preload in relation-types list, followed by another link-param.
    constexpr char s[] = R"(<url>; rel="preload"; as="font")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // preload in relation-types list, followed by character other
    // than ';' or ','
    constexpr char s[] = R"(<url>; rel="preload".)";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // preload in relation-types list, followed by ';' but it
    // terminates input
    constexpr char s[] = R"(<url>; rel="preload";)";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // preload in relation-types list, followed by ',' but it
    // terminates input
    constexpr char s[] = R"(<url>; rel="preload",)";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // preload in relation-types list but there is preceding white
    // space.
    constexpr char s[] = R"(<url>; rel=" preload")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // preload in relation-types list but there is trailing white
    // space.
    constexpr char s[] = R"(<url>; rel="preload ")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // backslash escaped characters in quoted-string
    constexpr char s[] = R"(<url>; rel=preload; title="foo\"baz\"bar")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // anchor="" is acceptable
    constexpr char s[] = R"(<url>; rel=preload; anchor="")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // With anchor="#foo", url should be ignored
    constexpr char s[] = R"(<url>; rel=preload; anchor="#foo")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // With anchor=f, url should be ignored
    constexpr char s[] = "<url>; rel=preload; anchor=f";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // First url is ignored With anchor="#foo", but url should be
    // accepted.
    constexpr char s[] =
        R"(<url>; rel=preload; anchor="#foo", <url2>; rel=preload)";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url2" == res[0].uri);
  }
  {
    // With loadpolicy="next", url should be ignored
    constexpr char s[] = R"(<url>; rel=preload; loadpolicy="next")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // url should be picked up if empty loadpolicy is specified
    constexpr char s[] = R"(<url>; rel=preload; loadpolicy="")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // case-insensitive match
    constexpr char s[] = R"(<url>; rel=preload; ANCHOR="#foo", <url2>; )"
                         R"(REL=PRELOAD, <url3>; REL="foo PRELOAD bar")";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(2 == res.size());
    CU_ASSERT("url2" == res[0].uri);
    CU_ASSERT("url3" == res[1].uri);
  }
  {
    // nopush at the end of input
    constexpr char s[] = "<url>; rel=preload; nopush";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // nopush followed by ';'
    constexpr char s[] = "<url>; rel=preload; nopush; foo";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // nopush followed by ','
    constexpr char s[] = "<url>; nopush; rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(0 == res.size());
  }
  {
    // string whose prefix is nopush
    constexpr char s[] = "<url>; nopushyes; rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
  {
    // rel=preload twice
    constexpr char s[] = "<url>; rel=preload; rel=preload";
    auto res = http2::parse_link_header(s, str_size(s));
    CU_ASSERT(1 == res.size());
    CU_ASSERT("url" == res[0].uri);
  }
}

void test_http2_path_join(void) {
  {
    auto base = StringRef::from_lit("/");
    auto rel = StringRef::from_lit("/");
    CU_ASSERT("/" == http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    auto base = StringRef::from_lit("/");
    auto rel = StringRef::from_lit("/alpha");
    CU_ASSERT("/alpha" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // rel ends with trailing '/'
    auto base = StringRef::from_lit("/");
    auto rel = StringRef::from_lit("/alpha/");
    CU_ASSERT("/alpha/" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // rel contains multiple components
    auto base = StringRef::from_lit("/");
    auto rel = StringRef::from_lit("/alpha/bravo");
    CU_ASSERT("/alpha/bravo" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // rel is relative
    auto base = StringRef::from_lit("/");
    auto rel = StringRef::from_lit("alpha/bravo");
    CU_ASSERT("/alpha/bravo" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // rel is relative and base ends without /, which means it refers
    // to file.
    auto base = StringRef::from_lit("/alpha");
    auto rel = StringRef::from_lit("bravo/charlie");
    CU_ASSERT("/bravo/charlie" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // rel contains repeated '/'s
    auto base = StringRef::from_lit("/");
    auto rel = StringRef::from_lit("/alpha/////bravo/////");
    CU_ASSERT("/alpha/bravo/" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // base ends with '/', so '..' eats 'bravo'
    auto base = StringRef::from_lit("/alpha/bravo/");
    auto rel = StringRef::from_lit("../charlie/delta");
    CU_ASSERT("/alpha/charlie/delta" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // base does not end with '/', so '..' eats 'alpha/bravo'
    auto base = StringRef::from_lit("/alpha/bravo");
    auto rel = StringRef::from_lit("../charlie");
    CU_ASSERT("/charlie" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // 'charlie' is eaten by following '..'
    auto base = StringRef::from_lit("/alpha/bravo/");
    auto rel = StringRef::from_lit("../charlie/../delta");
    CU_ASSERT("/alpha/delta" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // excessive '..' results in '/'
    auto base = StringRef::from_lit("/alpha/bravo/");
    auto rel = StringRef::from_lit("../../../");
    CU_ASSERT("/" == http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // excessive '..'  and  path component
    auto base = StringRef::from_lit("/alpha/bravo/");
    auto rel = StringRef::from_lit("../../../charlie");
    CU_ASSERT("/charlie" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // rel ends with '..'
    auto base = StringRef::from_lit("/alpha/bravo/");
    auto rel = StringRef::from_lit("charlie/..");
    CU_ASSERT("/alpha/bravo/" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // base empty and rel contains '..'
    auto base = StringRef{};
    auto rel = StringRef::from_lit("charlie/..");
    CU_ASSERT("/" == http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // '.' is ignored
    auto base = StringRef::from_lit("/");
    auto rel = StringRef::from_lit("charlie/././././delta");
    CU_ASSERT("/charlie/delta" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // trailing '.' is ignored
    auto base = StringRef::from_lit("/");
    auto rel = StringRef::from_lit("charlie/.");
    CU_ASSERT("/charlie/" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // query
    auto base = StringRef::from_lit("/");
    auto rel = StringRef::from_lit("/");
    auto relq = StringRef::from_lit("q");
    CU_ASSERT("/?q" == http2::path_join(base, StringRef{}, rel, relq));
  }
  {
    // empty rel and query
    auto base = StringRef::from_lit("/alpha");
    auto rel = StringRef{};
    auto relq = StringRef::from_lit("q");
    CU_ASSERT("/alpha?q" == http2::path_join(base, StringRef{}, rel, relq));
  }
  {
    // both rel and query are empty
    auto base = StringRef::from_lit("/alpha");
    auto baseq = StringRef::from_lit("r");
    auto rel = StringRef{};
    auto relq = StringRef{};
    CU_ASSERT("/alpha?r" == http2::path_join(base, baseq, rel, relq));
  }
  {
    // empty base
    auto base = StringRef{};
    auto rel = StringRef::from_lit("/alpha");
    CU_ASSERT("/alpha" ==
              http2::path_join(base, StringRef{}, rel, StringRef{}));
  }
  {
    // everything is empty
    CU_ASSERT("/" == http2::path_join(StringRef{}, StringRef{}, StringRef{},
                                      StringRef{}));
  }
  {
    // only baseq is not empty
    auto base = StringRef{};
    auto baseq = StringRef::from_lit("r");
    auto rel = StringRef{};
    CU_ASSERT("/?r" == http2::path_join(base, baseq, rel, StringRef{}));
  }
  {
    // path starts with multiple '/'s.
    auto base = StringRef{};
    auto baseq = StringRef{};
    auto rel = StringRef::from_lit("//alpha//bravo");
    auto relq = StringRef::from_lit("charlie");
    CU_ASSERT("/alpha/bravo?charlie" ==
              http2::path_join(base, baseq, rel, relq));
  }
}

void test_http2_normalize_path(void) {
  CU_ASSERT("/alpha/charlie" ==
            http2::normalize_path(
                StringRef::from_lit("/alpha/bravo/../charlie"), StringRef{}));

  CU_ASSERT("/alpha" ==
            http2::normalize_path(StringRef::from_lit("/a%6c%70%68%61"),
                                  StringRef{}));

  CU_ASSERT(
      "/alpha%2F%3A" ==
      http2::normalize_path(StringRef::from_lit("/alpha%2f%3a"), StringRef{}));

  CU_ASSERT("/%2F" ==
            http2::normalize_path(StringRef::from_lit("%2f"), StringRef{}));

  CU_ASSERT("/%f" ==
            http2::normalize_path(StringRef::from_lit("%f"), StringRef{}));

  CU_ASSERT("/%" ==
            http2::normalize_path(StringRef::from_lit("%"), StringRef{}));

  CU_ASSERT("/" == http2::normalize_path(StringRef{}, StringRef{}));

  CU_ASSERT("/alpha?bravo" ==
            http2::normalize_path(StringRef::from_lit("/alpha"),
                                  StringRef::from_lit("bravo")));
}

void test_http2_rewrite_clean_path(void) {
  BlockAllocator balloc(4096, 4096);

  // unreserved characters
  CU_ASSERT("/alpha/bravo/" ==
            http2::rewrite_clean_path(balloc,
                                      StringRef::from_lit("/alpha/%62ravo/")));

  // percent-encoding is converted to upper case.
  CU_ASSERT("/delta%3A" == http2::rewrite_clean_path(
                               balloc, StringRef::from_lit("/delta%3a")));

  // path component is normalized before mathcing
  CU_ASSERT(
      "/alpha/bravo/" ==
      http2::rewrite_clean_path(
          balloc, StringRef::from_lit("/alpha/charlie/%2e././bravo/delta/..")));

  CU_ASSERT("alpha%3a" ==
            http2::rewrite_clean_path(balloc, StringRef::from_lit("alpha%3a")));

  CU_ASSERT("" == http2::rewrite_clean_path(balloc, StringRef{}));

  CU_ASSERT(
      "/alpha?bravo" ==
      http2::rewrite_clean_path(balloc, StringRef::from_lit("//alpha?bravo")));
}

void test_http2_get_pure_path_component(void) {
  CU_ASSERT("/" == http2::get_pure_path_component(StringRef::from_lit("/")));

  CU_ASSERT("/foo" ==
            http2::get_pure_path_component(StringRef::from_lit("/foo")));

  CU_ASSERT("/bar" == http2::get_pure_path_component(
                          StringRef::from_lit("https://example.org/bar")));

  CU_ASSERT("/alpha" == http2::get_pure_path_component(StringRef::from_lit(
                            "https://example.org/alpha?q=a")));

  CU_ASSERT("/bravo" == http2::get_pure_path_component(StringRef::from_lit(
                            "https://example.org/bravo?q=a#fragment")));

  CU_ASSERT("" ==
            http2::get_pure_path_component(StringRef::from_lit("\x01\x02")));
}

void test_http2_construct_push_component(void) {
  BlockAllocator balloc(4096, 4096);
  StringRef base, uri;
  StringRef scheme, authority, path;

  base = StringRef::from_lit("/b/");
  uri = StringRef::from_lit("https://example.org/foo");

  CU_ASSERT(0 == http2::construct_push_component(balloc, scheme, authority,
                                                 path, base, uri));
  CU_ASSERT("https" == scheme);
  CU_ASSERT("example.org" == authority);
  CU_ASSERT("/foo" == path);

  scheme = StringRef{};
  authority = StringRef{};
  path = StringRef{};

  uri = StringRef::from_lit("/foo/bar?q=a");

  CU_ASSERT(0 == http2::construct_push_component(balloc, scheme, authority,
                                                 path, base, uri));
  CU_ASSERT("" == scheme);
  CU_ASSERT("" == authority);
  CU_ASSERT("/foo/bar?q=a" == path);

  scheme = StringRef{};
  authority = StringRef{};
  path = StringRef{};

  uri = StringRef::from_lit("foo/../bar?q=a");

  CU_ASSERT(0 == http2::construct_push_component(balloc, scheme, authority,
                                                 path, base, uri));
  CU_ASSERT("" == scheme);
  CU_ASSERT("" == authority);
  CU_ASSERT("/b/bar?q=a" == path);

  scheme = StringRef{};
  authority = StringRef{};
  path = StringRef{};

  uri = StringRef{};

  CU_ASSERT(0 == http2::construct_push_component(balloc, scheme, authority,
                                                 path, base, uri));
  CU_ASSERT("" == scheme);
  CU_ASSERT("" == authority);
  CU_ASSERT("/" == path);

  scheme = StringRef{};
  authority = StringRef{};
  path = StringRef{};

  uri = StringRef::from_lit("?q=a");

  CU_ASSERT(0 == http2::construct_push_component(balloc, scheme, authority,
                                                 path, base, uri));
  CU_ASSERT("" == scheme);
  CU_ASSERT("" == authority);
  CU_ASSERT("/b/?q=a" == path);
}

} // namespace shrpx
