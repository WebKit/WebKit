/*
 * Copyright (C) 2017, 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <WebCore/MIMETypeRegistry.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(MIMETypeRegistry, JSONMIMETypes)
{
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("application/json"_si));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("APPLICATION/JSON"_si));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("application/vnd.api+json"_si));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("anything/something+json"_si));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("text/json"_si));

    ASSERT_FALSE(MIMETypeRegistry::isSupportedJSONMIMEType("text/plain"_si));
    ASSERT_FALSE(MIMETypeRegistry::isSupportedJSONMIMEType("text/javascript"_si));
}

TEST(MIMETypeRegistry, JavaScriptMIMETypes)
{
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("text/javascript"_s));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("TEXT/JAVASCRIPT"_s));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("application/javascript"_s));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("application/ecmascript"_s));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("application/x-javascript"_s));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("application/x-ecmascript"_s));

    ASSERT_FALSE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("text/plain"_s));
    ASSERT_FALSE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("application/json"_s));
    ASSERT_FALSE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("foo/javascript"_s));
}

TEST(MIMETypeRegistry, CanShowMIMEType)
{
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/plain"_si));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/html"_si));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/xml"_si));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/foo"_si));

    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/json"_si));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("APPLICATION/JSON"_si));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/vnd.api+json"_si));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("anything/something+json"_si));

    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/javascript"_si));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("TEXT/JAVASCRIPT"_si));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/javascript"_si));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/ecmascript"_si));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/x-javascript"_si));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/x-ecmascript"_si));

    ASSERT_FALSE(MIMETypeRegistry::canShowMIMEType("foo/bar"_si));
    ASSERT_FALSE(MIMETypeRegistry::canShowMIMEType("text/vcard"_si));
}

TEST(MIMETypeRegistry, PreferredExtensionForMIMEType)
{
    EXPECT_TRUE(isEmpty(MIMETypeRegistry::preferredExtensionForMIMEType(nullptr)));
}

TEST(MIMETypeRegistry, ExtensionsForMIMEType)
{
    EXPECT_EQ(MIMETypeRegistry::extensionsForMIMEType(nullptr).size(), 0U);
}

TEST(MIMETypeRegistry, CorrectExtensionForMIMEType)
{
    EXPECT_TRUE(isEmpty(MIMETypeRegistry::correctExtensionForMIMEType(nullptr, "image/png"_si)));
    EXPECT_EQ("file.png"_s, MIMETypeRegistry::correctExtensionForMIMEType("file.png"_si, nullptr));
    EXPECT_EQ("file.png"_s, MIMETypeRegistry::correctExtensionForMIMEType("file.png"_si, "application/octet-stream"_si));
    EXPECT_EQ("file.html"_s, MIMETypeRegistry::correctExtensionForMIMEType("file.html"_si, "text/plain"_si));

    EXPECT_EQ("image.png"_s, MIMETypeRegistry::correctExtensionForMIMEType("image.png"_si, "image/png"_si));

    EXPECT_EQ("video.mp4"_s, MIMETypeRegistry::correctExtensionForMIMEType("video.gif"_si, "video/mp4"_si));
    EXPECT_EQ("file.png"_s, MIMETypeRegistry::correctExtensionForMIMEType("file.gif"_si, "image/png"_si));

    EXPECT_EQ("file.png"_s, MIMETypeRegistry::correctExtensionForMIMEType("file"_si, "image/png"_si));

#if PLATFORM(COCOA)
    EXPECT_EQ("archive.tar.gz"_s, MIMETypeRegistry::correctExtensionForMIMEType("archive.tar.gz"_si, "application/gzip"_si));

    EXPECT_EQ("resource.tgz"_s, MIMETypeRegistry::correctExtensionForMIMEType("resource.tgz"_si, "application/gzip"_si));

    EXPECT_EQ("resource.tgz"_s, MIMETypeRegistry::correctExtensionForMIMEType("resource.tgz"_si, "application/x-gzip"_si));
    EXPECT_EQ("file.zip"_s, MIMETypeRegistry::correctExtensionForMIMEType("file.zip"_si, "application/x-zip-compressed"_si));
    EXPECT_EQ("file.gz"_s, MIMETypeRegistry::correctExtensionForMIMEType("file.gz"_si, "application/gzip"_si));
    EXPECT_EQ("file.gz"_s, MIMETypeRegistry::correctExtensionForMIMEType("file.gz"_si, "application/x-gzip"_si));

    EXPECT_EQ("resource.TGZ"_s, MIMETypeRegistry::correctExtensionForMIMEType("resource.TGZ"_si, "application/gzip"_si));
    EXPECT_EQ("resource.tgz"_s, MIMETypeRegistry::correctExtensionForMIMEType("resource.tgz"_si, "Application/X-Gzip"_si));

    EXPECT_EQ("archive.cpgz"_s, MIMETypeRegistry::correctExtensionForMIMEType("archive.cpgz"_si, "application/zip"_si));
    EXPECT_EQ("archive.cpgz"_s, MIMETypeRegistry::correctExtensionForMIMEType("archive.cpgz"_si, "application/gzip"_si));
    EXPECT_EQ("data.tar"_s, MIMETypeRegistry::correctExtensionForMIMEType("data.tar"_si, "application/gzip"_si));
    EXPECT_EQ("data.tgz"_s, MIMETypeRegistry::correctExtensionForMIMEType("data.tgz"_si, "application/zip"_si));
    EXPECT_EQ("file.gz"_s, MIMETypeRegistry::correctExtensionForMIMEType("file.gz"_si, "application/zip"_si));
    EXPECT_EQ("app.jar"_s, MIMETypeRegistry::correctExtensionForMIMEType("app.jar"_si, "application/zip"_si));
#endif
}

} // namespace TestWebKitAPI
