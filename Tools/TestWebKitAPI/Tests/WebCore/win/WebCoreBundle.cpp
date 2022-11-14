/*
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
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

#include "Test.h"
#include <WebCore/WebCoreBundleWin.h>
#include <wtf/FileSystem.h>

class WebCoreBundleTest : public testing::Test {
public:
    void SetUp() override
    {
        WCHAR buffer[MAX_PATH];
        DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        if (!length || (length == MAX_PATH && GetLastError() == ERROR_INSUFFICIENT_BUFFER))
            return;

        String path(buffer, length);
        m_root = FileSystem::pathByAppendingComponent(FileSystem::parentPath(path), "WebKit.resources"_s);
    }

protected:
    String m_root;
};

TEST_F(WebCoreBundleTest, BundleRootPath)
{
    auto actual = WebCore::webKitBundlePath();
    EXPECT_STREQ(m_root.utf8().data(), actual.utf8().data());
}

TEST_F(WebCoreBundleTest, BundlePathFromPath)
{
    auto actual = WebCore::webKitBundlePath("WebInspectorUI\\Protocol\\InspectorBackendCommands.js"_s);
    auto expected = FileSystem::pathByAppendingComponents(m_root, { "WebInspectorUI"_s, "Protocol"_s, "InspectorBackendCommands.js"_s });
    EXPECT_STREQ(expected.utf8().data(), actual.utf8().data());
}

TEST_F(WebCoreBundleTest, BundlePathFromNameTypeDirectory)
{
    auto actual = WebCore::webKitBundlePath("InspectorBackendCommands"_s, "js"_s, "WebInspectorUI\\Protocol"_s);
    auto expected = FileSystem::pathByAppendingComponents(m_root, { "WebInspectorUI"_s, "Protocol"_s, "InspectorBackendCommands.js"_s });
    EXPECT_STREQ(expected.utf8().data(), actual.utf8().data());

    actual = WebCore::webKitBundlePath("Localizable"_s, "strings"_s, ""_s);
    expected = FileSystem::pathByAppendingComponents(m_root, { "en.lproj"_s, "Localizable.strings"_s });
    EXPECT_STREQ(expected.utf8().data(), actual.utf8().data());

#if !ENABLE(MODERN_MEDIA_CONTROLS)
    actual = WebCore::webKitBundlePath("mediaControlsLocalizedStrings"_s, "js"_s, ""_s);
    expected = FileSystem::pathByAppendingComponents(m_root, { "en.lproj"_s, "mediaControlsLocalizedStrings.js"_s });
    EXPECT_STREQ(expected.utf8().data(), actual.utf8().data());
#endif

    actual = WebCore::webKitBundlePath("file-does-not"_s, "exist"_s, "file"_s);
    expected = emptyString();
    EXPECT_STREQ(expected.utf8().data(), actual.utf8().data());
}

TEST_F(WebCoreBundleTest, BundlePathFromComponents)
{
    auto actual = WebCore::webKitBundlePath({ "WebInspectorUI"_s, "Protocol"_s, "InspectorBackendCommands.js"_s });
    auto expected = FileSystem::pathByAppendingComponents(m_root, { "WebInspectorUI"_s, "Protocol"_s, "InspectorBackendCommands.js"_s });
    EXPECT_STREQ(expected.utf8().data(), actual.utf8().data());
}

