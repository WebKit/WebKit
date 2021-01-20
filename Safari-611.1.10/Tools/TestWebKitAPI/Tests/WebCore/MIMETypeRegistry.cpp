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
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("application/json"));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("APPLICATION/JSON"));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("application/vnd.api+json"));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJSONMIMEType("anything/something+json"));

    ASSERT_FALSE(MIMETypeRegistry::isSupportedJSONMIMEType("text/plain"));
    ASSERT_FALSE(MIMETypeRegistry::isSupportedJSONMIMEType("text/json"));
    ASSERT_FALSE(MIMETypeRegistry::isSupportedJSONMIMEType("text/javascript"));
}

TEST(MIMETypeRegistry, JavaScriptMIMETypes)
{
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("text/javascript"));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("TEXT/JAVASCRIPT"));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("application/javascript"));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("application/ecmascript"));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("application/x-javascript"));
    ASSERT_TRUE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("application/x-ecmascript"));

    ASSERT_FALSE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("text/plain"));
    ASSERT_FALSE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("application/json"));
    ASSERT_FALSE(MIMETypeRegistry::isSupportedJavaScriptMIMEType("foo/javascript"));
}

TEST(MIMETypeRegistry, CanShowMIMEType)
{
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/plain"));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/html"));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/xml"));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/foo"));

    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/json"));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("APPLICATION/JSON"));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/vnd.api+json"));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("anything/something+json"));

    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("text/javascript"));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("TEXT/JAVASCRIPT"));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/javascript"));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/ecmascript"));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/x-javascript"));
    ASSERT_TRUE(MIMETypeRegistry::canShowMIMEType("application/x-ecmascript"));

    ASSERT_FALSE(MIMETypeRegistry::canShowMIMEType("foo/bar"));
    ASSERT_FALSE(MIMETypeRegistry::canShowMIMEType("text/vcard"));
}

} // namespace TestWebKitAPI
