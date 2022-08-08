/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("application/json"_s));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("APPLICATION/JSON"_s));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("application/vnd.api+json"_s));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("anything/something+json"_s));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("text/json"_s));

    ASSERT_FALSE(MIMETypeRegistry::isSupportedJSONMIMEType("text/plain"_s));
    ASSERT_FALSE(MIMETypeRegistry::isSupportedJSONMIMEType("text/javascript"_s));
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
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/plain"_s));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/html"_s));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/xml"_s));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/foo"_s));

    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/json"_s));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("APPLICATION/JSON"_s));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/vnd.api+json"_s));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("anything/something+json"_s));

    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/javascript"_s));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("TEXT/JAVASCRIPT"_s));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/javascript"_s));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/ecmascript"_s));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/x-javascript"_s));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/x-ecmascript"_s));

    ASSERT_FALSE(MIMETypeRegistry::canShowMIMEType("foo/bar"_s));
    ASSERT_FALSE(MIMETypeRegistry::canShowMIMEType("text/vcard"_s));
}

} // namespace TestWebKitAPI
