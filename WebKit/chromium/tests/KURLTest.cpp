/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Basic tests that verify our KURL's interface behaves the same as the
// original KURL's.

#include "config.h"

#include <gtest/gtest.h>

#include "KURL.h"

namespace {

// Output stream operator so gTest's macros work with WebCore strings.
std::ostream& operator<<(std::ostream& out, const WebCore::String& str)
{
    return str.isEmpty() ? out : out << str.utf8().data();
}

struct ComponentCase {
    const char* url;
    const char* protocol;
    const char* host;
    const int port;
    const char* user;
    const char* pass;
    const char* path;
    const char* lastPath;
    const char* query;
    const char* ref;
};

// Test the cases where we should be the same as WebKit's old KURL.
TEST(KURLTest, SameGetters)
{
    struct GetterCase {
        const char* url;
        const char* protocol;
        const char* host;
        int port;
        const char* user;
        const char* pass;
        const char* lastPathComponent;
        const char* query;
        const char* ref;
        bool hasRef;
    } cases[] = {
        {"http://www.google.com/foo/blah?bar=baz#ref", "http", "www.google.com", 0, "", 0, "blah", "bar=baz", "ref", true},
        {"http://foo.com:1234/foo/bar/", "http", "foo.com", 1234, "", 0, "bar", 0, 0, false},
        {"http://www.google.com?#", "http", "www.google.com", 0, "", 0, 0, "", "", true},
        {"https://me:pass@google.com:23#foo", "https", "google.com", 23, "me", "pass", 0, 0, "foo", true},
        {"javascript:hello!//world", "javascript", "", 0, "", 0, "world", 0, 0, false},
    };

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
        // UTF-8
        WebCore::KURL kurl(WebCore::ParsedURLString, cases[i].url);

        EXPECT_EQ(cases[i].protocol, kurl.protocol());
        EXPECT_EQ(cases[i].host, kurl.host());
        EXPECT_EQ(cases[i].port, kurl.port());
        EXPECT_EQ(cases[i].user, kurl.user());
        EXPECT_EQ(cases[i].pass, kurl.pass());
        EXPECT_EQ(cases[i].lastPathComponent, kurl.lastPathComponent());
        EXPECT_EQ(cases[i].query, kurl.query());
        EXPECT_EQ(cases[i].ref, kurl.fragmentIdentifier());
        EXPECT_EQ(cases[i].hasRef, kurl.hasFragmentIdentifier());

        // UTF-16
        WebCore::String utf16(cases[i].url);
        kurl = WebCore::KURL(WebCore::ParsedURLString, utf16);

        EXPECT_EQ(cases[i].protocol, kurl.protocol());
        EXPECT_EQ(cases[i].host, kurl.host());
        EXPECT_EQ(cases[i].port, kurl.port());
        EXPECT_EQ(cases[i].user, kurl.user());
        EXPECT_EQ(cases[i].pass, kurl.pass());
        EXPECT_EQ(cases[i].lastPathComponent, kurl.lastPathComponent());
        EXPECT_EQ(cases[i].query, kurl.query());
        EXPECT_EQ(cases[i].ref, kurl.fragmentIdentifier());
        EXPECT_EQ(cases[i].hasRef, kurl.hasFragmentIdentifier());
    }
}

// Test a few cases where we're different just to make sure we give reasonable
// output.
TEST(KURLTest, DifferentGetters)
{
    ComponentCase cases[] = {
        // url                                    protocol      host        port  user  pass    path                lastPath  query      ref

        // Old WebKit allows references and queries in what we call "path" URLs
        // like javascript, so the path here will only consist of "hello!".
        {"javascript:hello!?#/\\world",           "javascript", "",         0,    "",   0,      "hello!?#/\\world", "world",  0,         0},

        // Old WebKit doesn't handle "parameters" in paths, so will
        // disagree with us about where the path is for this URL.
        {"http://a.com/hello;world",              "http",       "a.com",    0,    "",   0,      "/hello;world",     "hello",  0,         0},

        // WebKit doesn't like UTF-8 or UTF-16 input.
        {"http://\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd/", "http", "xn--6qqa088eba", 0, "", 0, "/",       0,        0,         0},

        // WebKit %-escapes non-ASCII characters in reference, but we don't.
        {"http://www.google.com/foo/blah?bar=baz#\xce\xb1\xce\xb2", "http", "www.google.com", 0, "", 0, "/foo/blah/", "blah", "bar=baz", "\xce\xb1\xce\xb2"},
    };

    for (size_t i = 0; i < arraysize(cases); i++) {
        WebCore::KURL kurl(WebCore::ParsedURLString, cases[i].url);

        EXPECT_EQ(cases[i].protocol, kurl.protocol());
        EXPECT_EQ(cases[i].host, kurl.host());
        EXPECT_EQ(cases[i].port, kurl.port());
        EXPECT_EQ(cases[i].user, kurl.user());
        EXPECT_EQ(cases[i].pass, kurl.pass());
        EXPECT_EQ(cases[i].lastPath, kurl.lastPathComponent());
        EXPECT_EQ(cases[i].query, kurl.query());
        // Want to compare UCS-16 refs (or to null).
        if (cases[i].ref)
            EXPECT_EQ(WebCore::String::fromUTF8(cases[i].ref), kurl.fragmentIdentifier());
        else
            EXPECT_TRUE(kurl.fragmentIdentifier().isNull());
    }
}

// Ensures that both ASCII and UTF-8 canonical URLs are handled properly and we
// get the correct string object out.
TEST(KURLTest, UTF8)
{
    const char asciiURL[] = "http://foo/bar#baz";
    WebCore::KURL asciiKURL(WebCore::ParsedURLString, asciiURL);
    EXPECT_TRUE(asciiKURL.string() == WebCore::String(asciiURL));

    // When the result is ASCII, we should get an ASCII String. Some
    // code depends on being able to compare the result of the .string()
    // getter with another String, and the isASCIIness of the two
    // strings must match for these functions (like equalIgnoringCase).
    EXPECT_TRUE(WebCore::equalIgnoringCase(asciiKURL, WebCore::String(asciiURL)));

    // Reproduce code path in FrameLoader.cpp -- equalIgnoringCase implicitly
    // expects gkurl.protocol() to have been created as ascii.
    WebCore::KURL mailto(WebCore::ParsedURLString, "mailto:foo@foo.com");
    EXPECT_TRUE(WebCore::equalIgnoringCase(mailto.protocol(), "mailto"));

    const char utf8URL[] = "http://foo/bar#\xe4\xbd\xa0\xe5\xa5\xbd";
    WebCore::KURL utf8KURL(WebCore::ParsedURLString, utf8URL);

    EXPECT_TRUE(utf8KURL.string() == WebCore::String::fromUTF8(utf8URL));
}

TEST(KURLTest, Setters)
{
    // Replace the starting URL with the given components one at a time and
    // verify that we're always the same as the old KURL.
    //
    // Note that old KURL won't canonicalize the default port away, so we
    // can't set setting the http port to "80" (or even "0").
    //
    // We also can't test clearing the query.
    //
    // The format is every other row is a test, and the row that follows it is the
    // expected result.
    struct ExpectedComponentCase {
        const char* url;
        const char* protocol;
        const char* host;
        const int port;
        const char* user;
        const char* pass;
        const char* path;
        const char* query;
        const char* ref;

        // The full expected URL with the given "set" applied.
        const char* expectedProtocol;
        const char* expectedHost;
        const char* expectedPort;
        const char* expectedUser;
        const char* expectedPass;
        const char* expectedPath;
        const char* expectedQuery;
        const char* expectedRef;
    } cases[] = {
         // url                                   protocol      host               port  user  pass    path            query      ref
        {"http://www.google.com/",                "https",      "news.google.com", 8888, "me", "pass", "/foo",         "?q=asdf", "heehee",
                                                  "https://www.google.com/",
                                                                "https://news.google.com/",
                                                                                   "https://news.google.com:8888/",
                                                                                         "https://me@news.google.com:8888/",
                                                                                               "https://me:pass@news.google.com:8888/",
                                                                                                       "https://me:pass@news.google.com:8888/foo",
                                                                                                                       "https://me:pass@news.google.com:8888/foo?q=asdf",
                                                                                                                                  "https://me:pass@news.google.com:8888/foo?q=asdf#heehee"},

        {"https://me:pass@google.com:88/a?f#b",   "http",       "goo.com",         92,   "",   "",     "/",            0,      "",
                                                  "http://me:pass@google.com:88/a?f#b",
                                                                "http://me:pass@goo.com:88/a?f#b",
                                                                                   "http://me:pass@goo.com:92/a?f#b",
                                                                                         "http://:pass@goo.com:92/a?f#b",
                                                                                               "http://goo.com:92/a?f#b",
                                                                                                        "http://goo.com:92/?f#b",
                                                                                                                       "http://goo.com:92/#b",
                                                                                                                                  "https://goo.com:92/"},
    };

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
        WebCore::KURL kurl(WebCore::ParsedURLString, cases[i].url);

        kurl.setProtocol(cases[i].protocol);
        EXPECT_STREQ(cases[i].expectedProtocol, kurl.string().utf8().data());

        kurl.setHost(cases[i].host);
        EXPECT_STREQ(cases[i].expectedHost, kurl.string().utf8().data());

        kurl.setPort(cases[i].port);
        EXPECT_STREQ(cases[i].expectedPort, kurl.string().utf8().data());

        kurl.setUser(cases[i].user);
        EXPECT_STREQ(cases[i].expectedUser, kurl.string().utf8().data());

        kurl.setPass(cases[i].pass);
        EXPECT_STREQ(cases[i].expectedPass, kurl.string().utf8().data());

        kurl.setPath(cases[i].path);
        EXPECT_STREQ(cases[i].expectedPath, kurl.string().utf8().data());

        kurl.setQuery(cases[i].query);
        EXPECT_STREQ(cases[i].expectedQuery, kurl.string().utf8().data());

        // Refs are tested below. On the Safari 3.1 branch, we don't match their
        // KURL since we integrated a fix from their trunk.
    }
}

// Tests that KURL::decodeURLEscapeSequences works as expected
#if USE(GOOGLEURL)
TEST(KURLTest, Decode)
{
    struct DecodeCase {
        const char* input;
        const char* output;
    } decodeCases[] = {
        {"hello, world", "hello, world"},
        {"%01%02%03%04%05%06%07%08%09%0a%0B%0C%0D%0e%0f/", "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0B\x0C\x0D\x0e\x0f/"},
        {"%10%11%12%13%14%15%16%17%18%19%1a%1B%1C%1D%1e%1f/", "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1B\x1C\x1D\x1e\x1f/"},
        {"%20%21%22%23%24%25%26%27%28%29%2a%2B%2C%2D%2e%2f/", " !\"#$%&'()*+,-.//"},
        {"%30%31%32%33%34%35%36%37%38%39%3a%3B%3C%3D%3e%3f/", "0123456789:;<=>?/"},
        {"%40%41%42%43%44%45%46%47%48%49%4a%4B%4C%4D%4e%4f/", "@ABCDEFGHIJKLMNO/"},
        {"%50%51%52%53%54%55%56%57%58%59%5a%5B%5C%5D%5e%5f/", "PQRSTUVWXYZ[\\]^_/"},
        {"%60%61%62%63%64%65%66%67%68%69%6a%6B%6C%6D%6e%6f/", "`abcdefghijklmno/"},
        {"%70%71%72%73%74%75%76%77%78%79%7a%7B%7C%7D%7e%7f/", "pqrstuvwxyz{|}~\x7f/"},
          // Test un-UTF-8-ization.
        {"%e4%bd%a0%e5%a5%bd", "\xe4\xbd\xa0\xe5\xa5\xbd"},
    };

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(decodeCases); i++) {
        WebCore::String input(decodeCases[i].input);
        WebCore::String str = WebCore::decodeURLEscapeSequences(input);
        EXPECT_STREQ(decodeCases[i].output, str.utf8().data());
    }

    // Our decode should decode %00
    WebCore::String zero = WebCore::decodeURLEscapeSequences("%00");
    EXPECT_STRNE("%00", zero.utf8().data());

    // Test the error behavior for invalid UTF-8 (we differ from WebKit here).
    WebCore::String invalid = WebCore::decodeURLEscapeSequences(
        "%e4%a0%e5%a5%bd");
    char16 invalidExpectedHelper[4] = { 0x00e4, 0x00a0, 0x597d, 0 };
    WebCore::String invalidExpected(
        reinterpret_cast<const ::UChar*>(invalidExpectedHelper),
        3);
    EXPECT_EQ(invalidExpected, invalid);
}
#endif

TEST(KURLTest, Encode)
{
    // Also test that it gets converted to UTF-8 properly.
    char16 wideInputHelper[3] = { 0x4f60, 0x597d, 0 };
    WebCore::String wideInput(
        reinterpret_cast<const ::UChar*>(wideInputHelper), 2);
    WebCore::String wideReference("\xe4\xbd\xa0\xe5\xa5\xbd", 6);
    WebCore::String wideOutput =
        WebCore::encodeWithURLEscapeSequences(wideInput);
    EXPECT_EQ(wideReference, wideOutput);

    // Our encode only escapes NULLs for safety (see the implementation for
    // more), so we only bother to test a few cases.
    WebCore::String input(
        "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f", 16);
    WebCore::String reference(
        "%00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f", 18);
    WebCore::String output = WebCore::encodeWithURLEscapeSequences(input);
    EXPECT_EQ(reference, output);
}

TEST(KURLTest, ResolveEmpty)
{
    WebCore::KURL emptyBase;

    // WebKit likes to be able to resolve absolute input agains empty base URLs,
    // which would normally be invalid since the base URL is invalid.
    const char abs[] = "http://www.google.com/";
    WebCore::KURL resolveAbs(emptyBase, abs);
    EXPECT_TRUE(resolveAbs.isValid());
    EXPECT_STREQ(abs, resolveAbs.string().utf8().data());

    // Resolving a non-relative URL agains the empty one should still error.
    const char rel[] = "foo.html";
    WebCore::KURL resolveErr(emptyBase, rel);
    EXPECT_FALSE(resolveErr.isValid());
}

// WebKit will make empty URLs and set components on them. kurl doesn't allow
// replacements on invalid URLs, but here we do.
TEST(KURLTest, ReplaceInvalid)
{
    WebCore::KURL kurl;

    EXPECT_FALSE(kurl.isValid());
    EXPECT_TRUE(kurl.isEmpty());
    EXPECT_STREQ("", kurl.string().utf8().data());

    kurl.setProtocol("http");
    // GKURL will say that a URL with just a scheme is invalid, KURL will not.
#if USE(GOOGLEURL)
    EXPECT_FALSE(kurl.isValid());
#else
    EXPECT_TRUE(kurl.isValid());
#endif
    EXPECT_FALSE(kurl.isEmpty());
    // At this point, we do things slightly differently if there is only a scheme.
    // We check the results here to make it more obvious what is going on, but it
    // shouldn't be a big deal if these change.
#if USE(GOOGLEURL)
    EXPECT_STREQ("http:", kurl.string().utf8().data());
#else
    EXPECT_STREQ("http:/", kurl.string().utf8().data());
#endif

    kurl.setHost("www.google.com");
    EXPECT_TRUE(kurl.isValid());
    EXPECT_FALSE(kurl.isEmpty());
    EXPECT_STREQ("http://www.google.com/", kurl.string().utf8().data());

    kurl.setPort(8000);
    EXPECT_TRUE(kurl.isValid());
    EXPECT_FALSE(kurl.isEmpty());
    EXPECT_STREQ("http://www.google.com:8000/", kurl.string().utf8().data());

    kurl.setPath("/favicon.ico");
    EXPECT_TRUE(kurl.isValid());
    EXPECT_FALSE(kurl.isEmpty());
    EXPECT_STREQ("http://www.google.com:8000/favicon.ico", kurl.string().utf8().data());

    // Now let's test that giving an invalid replacement still fails.
#if USE(GOOGLEURL)
    kurl.setProtocol("f/sj#@");
    EXPECT_FALSE(kurl.isValid());
#endif
}

TEST(KURLTest, Path)
{
    const char initial[] = "http://www.google.com/path/foo";
    WebCore::KURL kurl(WebCore::ParsedURLString, initial);

    // Clear by setting a null string.
    WebCore::String nullString;
    EXPECT_TRUE(nullString.isNull());
    kurl.setPath(nullString);
    EXPECT_STREQ("http://www.google.com/", kurl.string().utf8().data());
}

// Test that setting the query to different things works. Thq query is handled
// a littler differently than some of the other components.
TEST(KURLTest, Query)
{
    const char initial[] = "http://www.google.com/search?q=awesome";
    WebCore::KURL kurl(WebCore::ParsedURLString, initial);

    // Clear by setting a null string.
    WebCore::String nullString;
    EXPECT_TRUE(nullString.isNull());
    kurl.setQuery(nullString);
    EXPECT_STREQ("http://www.google.com/search", kurl.string().utf8().data());

    // Clear by setting an empty string.
    kurl = WebCore::KURL(WebCore::ParsedURLString, initial);
    WebCore::String emptyString("");
    EXPECT_FALSE(emptyString.isNull());
    kurl.setQuery(emptyString);
    EXPECT_STREQ("http://www.google.com/search?", kurl.string().utf8().data());

    // Set with something that begins in a question mark.
    const char question[] = "?foo=bar";
    kurl.setQuery(question);
    EXPECT_STREQ("http://www.google.com/search?foo=bar",
                 kurl.string().utf8().data());

    // Set with something that doesn't begin in a question mark.
    const char query[] = "foo=bar";
    kurl.setQuery(query);
    EXPECT_STREQ("http://www.google.com/search?foo=bar",
                 kurl.string().utf8().data());
}

TEST(KURLTest, Ref)
{
    WebCore::KURL kurl(WebCore::ParsedURLString, "http://foo/bar#baz");

    // Basic ref setting.
    WebCore::KURL cur(WebCore::ParsedURLString, "http://foo/bar");
    cur.setFragmentIdentifier("asdf");
    EXPECT_STREQ("http://foo/bar#asdf", cur.string().utf8().data());
    cur = kurl;
    cur.setFragmentIdentifier("asdf");
    EXPECT_STREQ("http://foo/bar#asdf", cur.string().utf8().data());

    // Setting a ref to the empty string will set it to "#".
    cur = WebCore::KURL(WebCore::ParsedURLString, "http://foo/bar");
    cur.setFragmentIdentifier("");
    EXPECT_STREQ("http://foo/bar#", cur.string().utf8().data());
    cur = kurl;
    cur.setFragmentIdentifier("");
    EXPECT_STREQ("http://foo/bar#", cur.string().utf8().data());

    // Setting the ref to the null string will clear it altogether.
    cur = WebCore::KURL(WebCore::ParsedURLString, "http://foo/bar");
    cur.setFragmentIdentifier(WebCore::String());
    EXPECT_STREQ("http://foo/bar", cur.string().utf8().data());
    cur = kurl;
    cur.setFragmentIdentifier(WebCore::String());
    EXPECT_STREQ("http://foo/bar", cur.string().utf8().data());
}

TEST(KURLTest, Empty)
{
    WebCore::KURL kurl;

    // First test that regular empty URLs are the same.
    EXPECT_TRUE(kurl.isEmpty());
    EXPECT_FALSE(kurl.isValid());
    EXPECT_TRUE(kurl.isNull());
    EXPECT_TRUE(kurl.string().isNull());
    EXPECT_TRUE(kurl.string().isEmpty());

    // Test resolving a null URL on an empty string.
    WebCore::KURL kurl2(kurl, "");
    EXPECT_FALSE(kurl2.isNull());
    EXPECT_TRUE(kurl2.isEmpty());
    EXPECT_FALSE(kurl2.isValid());
    EXPECT_FALSE(kurl2.string().isNull());
    EXPECT_TRUE(kurl2.string().isEmpty());
    EXPECT_FALSE(kurl2.string().isNull());
    EXPECT_TRUE(kurl2.string().isEmpty());

    // Resolve the null URL on a null string.
    WebCore::KURL kurl22(kurl, WebCore::String());
    EXPECT_FALSE(kurl22.isNull());
    EXPECT_TRUE(kurl22.isEmpty());
    EXPECT_FALSE(kurl22.isValid());
    EXPECT_FALSE(kurl22.string().isNull());
    EXPECT_TRUE(kurl22.string().isEmpty());
    EXPECT_FALSE(kurl22.string().isNull());
    EXPECT_TRUE(kurl22.string().isEmpty());

    // Test non-hierarchical schemes resolving. The actual URLs will be different.
    // WebKit's one will set the string to "something.gif" and we'll set it to an
    // empty string. I think either is OK, so we just check our behavior.
#if USE(GOOGLEURL)
    WebCore::KURL kurl3(WebCore::KURL(WebCore::ParsedURLString, "data:foo"),
                        "something.gif");
    EXPECT_TRUE(kurl3.isEmpty());
    EXPECT_FALSE(kurl3.isValid());
#endif

    // Test for weird isNull string input,
    // see: http://bugs.webkit.org/show_bug.cgi?id=16487
    WebCore::KURL kurl4(WebCore::ParsedURLString, kurl.string());
    EXPECT_TRUE(kurl4.isEmpty());
    EXPECT_FALSE(kurl4.isValid());
    EXPECT_TRUE(kurl4.string().isNull());
    EXPECT_TRUE(kurl4.string().isEmpty());

    // Resolving an empty URL on an invalid string.
    WebCore::KURL kurl5(WebCore::KURL(), "foo.js");
    // We'll be empty in this case, but KURL won't be. Should be OK.
    // EXPECT_EQ(kurl5.isEmpty(), kurl5.isEmpty());
    // EXPECT_EQ(kurl5.string().isEmpty(), kurl5.string().isEmpty());
    EXPECT_FALSE(kurl5.isValid());
    EXPECT_FALSE(kurl5.string().isNull());

    // Empty string as input
    WebCore::KURL kurl6(WebCore::ParsedURLString, "");
    EXPECT_TRUE(kurl6.isEmpty());
    EXPECT_FALSE(kurl6.isValid());
    EXPECT_FALSE(kurl6.string().isNull());
    EXPECT_TRUE(kurl6.string().isEmpty());

    // Non-empty but invalid C string as input.
    WebCore::KURL kurl7(WebCore::ParsedURLString, "foo.js");
    // WebKit will actually say this URL has the string "foo.js" but is invalid.
    // We don't do that.
    // EXPECT_EQ(kurl7.isEmpty(), kurl7.isEmpty());
    EXPECT_FALSE(kurl7.isValid());
    EXPECT_FALSE(kurl7.string().isNull());
}

TEST(KURLTest, UserPass)
{
    const char* src = "http://user:pass@google.com/";
    WebCore::KURL kurl(WebCore::ParsedURLString, src);

    // Clear just the username.
    kurl.setUser("");
    EXPECT_EQ("http://:pass@google.com/", kurl.string());

    // Clear just the password.
    kurl = WebCore::KURL(WebCore::ParsedURLString, src);
    kurl.setPass("");
    EXPECT_EQ("http://user@google.com/", kurl.string());

    // Now clear both.
    kurl.setUser("");
    EXPECT_EQ("http://google.com/", kurl.string());
}

TEST(KURLTest, Offsets)
{
    const char* src1 = "http://user:pass@google.com/foo/bar.html?baz=query#ref";
    WebCore::KURL kurl1(WebCore::ParsedURLString, src1);

    EXPECT_EQ(17u, kurl1.hostStart());
    EXPECT_EQ(27u, kurl1.hostEnd());
    EXPECT_EQ(27u, kurl1.pathStart());
    EXPECT_EQ(40u, kurl1.pathEnd());
    EXPECT_EQ(32u, kurl1.pathAfterLastSlash());

    const char* src2 = "http://google.com/foo/";
    WebCore::KURL kurl2(WebCore::ParsedURLString, src2);

    EXPECT_EQ(7u, kurl2.hostStart());
    EXPECT_EQ(17u, kurl2.hostEnd());
    EXPECT_EQ(17u, kurl2.pathStart());
    EXPECT_EQ(22u, kurl2.pathEnd());
    EXPECT_EQ(22u, kurl2.pathAfterLastSlash());

    const char* src3 = "javascript:foobar";
    WebCore::KURL kurl3(WebCore::ParsedURLString, src3);

    EXPECT_EQ(11u, kurl3.hostStart());
    EXPECT_EQ(11u, kurl3.hostEnd());
    EXPECT_EQ(11u, kurl3.pathStart());
    EXPECT_EQ(17u, kurl3.pathEnd());
    EXPECT_EQ(11u, kurl3.pathAfterLastSlash());
}

TEST(KURLTest, DeepCopy)
{
    const char url[] = "http://www.google.com/";
    WebCore::KURL src(WebCore::ParsedURLString, url);
    EXPECT_TRUE(src.string() == url); // This really just initializes the cache.
    WebCore::KURL dest = src.copy();
    EXPECT_TRUE(dest.string() == url); // This really just initializes the cache.

    // The pointers should be different for both UTF-8 and UTF-16.
    EXPECT_NE(dest.string().characters(), src.string().characters());
    EXPECT_NE(dest.utf8String().data(), src.utf8String().data());
}

TEST(KURLTest, ProtocolIs)
{
    WebCore::KURL url1(WebCore::ParsedURLString, "foo://bar");
    EXPECT_TRUE(url1.protocolIs("foo"));
    EXPECT_FALSE(url1.protocolIs("foo-bar"));

    WebCore::KURL url2(WebCore::ParsedURLString, "foo-bar:");
    EXPECT_TRUE(url2.protocolIs("foo-bar"));
    EXPECT_FALSE(url2.protocolIs("foo"));
}

} // namespace
