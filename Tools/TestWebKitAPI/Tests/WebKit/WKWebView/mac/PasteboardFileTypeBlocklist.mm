/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"

#if PLATFORM(MAC)

#import <WebCore/PlatformPasteboard.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(PlatformPasteboard, IsFilePasteboardTypeExactMatches)
{
    EXPECT_TRUE(WebCore::PlatformPasteboard::isFilePasteboardType("public.file-url"_s));
    EXPECT_TRUE(WebCore::PlatformPasteboard::isFilePasteboardType("NSFilenamesPboardType"_s));
    EXPECT_TRUE(WebCore::PlatformPasteboard::isFilePasteboardType("Apple files promise pasteboard type"_s));
}

TEST(PlatformPasteboard, IsFilePasteboardTypeCaseVariants)
{
    EXPECT_TRUE(WebCore::PlatformPasteboard::isFilePasteboardType("PUBLIC.FILE-URL"_s));
    EXPECT_TRUE(WebCore::PlatformPasteboard::isFilePasteboardType("Public.File-Url"_s));
    EXPECT_TRUE(WebCore::PlatformPasteboard::isFilePasteboardType("public.FILE-url"_s));
    EXPECT_TRUE(WebCore::PlatformPasteboard::isFilePasteboardType("nsfilenamespboardtype"_s));
    EXPECT_TRUE(WebCore::PlatformPasteboard::isFilePasteboardType("NSFILENAMESPBOARDTYPE"_s));
    EXPECT_TRUE(WebCore::PlatformPasteboard::isFilePasteboardType("APPLE FILES PROMISE PASTEBOARD TYPE"_s));
    EXPECT_TRUE(WebCore::PlatformPasteboard::isFilePasteboardType("apple files promise pasteboard type"_s));
}

TEST(PlatformPasteboard, IsFilePasteboardTypeCorePasteboardFlavor)
{
    // 'furl' = 0x6675726C = public.file-url
    EXPECT_TRUE(WebCore::PlatformPasteboard::isFilePasteboardType("CorePasteboardFlavorType 0x6675726C"_s));
    EXPECT_TRUE(WebCore::PlatformPasteboard::isFilePasteboardType("CorePasteboardFlavorType 0x6675726c"_s));
}

TEST(PlatformPasteboard, IsFilePasteboardTypeDynamicUTI)
{
    // Dynamic UTI encoding of NSFilenamesPboardType
    EXPECT_TRUE(WebCore::PlatformPasteboard::isFilePasteboardType("dyn.ah62d4rv4gu8y6y4grf0gn5xbrzw1gydcr7u1e3cytf2gn"_s));
}

TEST(PlatformPasteboard, IsFilePasteboardTypeNonFileTypes)
{
    EXPECT_FALSE(WebCore::PlatformPasteboard::isFilePasteboardType("public.utf8-plain-text"_s));
    EXPECT_FALSE(WebCore::PlatformPasteboard::isFilePasteboardType("public.html"_s));
    EXPECT_FALSE(WebCore::PlatformPasteboard::isFilePasteboardType("public.url"_s));
    EXPECT_FALSE(WebCore::PlatformPasteboard::isFilePasteboardType("public.png"_s));
    EXPECT_FALSE(WebCore::PlatformPasteboard::isFilePasteboardType("com.apple.webarchive"_s));
    EXPECT_FALSE(WebCore::PlatformPasteboard::isFilePasteboardType("Apple URL pasteboard type"_s));
    EXPECT_FALSE(WebCore::PlatformPasteboard::isFilePasteboardType(""_s));
}

TEST(PlatformPasteboard, SetTypesRejectsFileTypes)
{
    WebCore::PlatformPasteboard pasteboard("com.apple.WebKit.test.file-type-blocklist"_s);

    EXPECT_EQ(0, pasteboard.addTypes({ "PUBLIC.FILE-URL"_s }));
    EXPECT_EQ(0, pasteboard.addTypes({ "CorePasteboardFlavorType 0x6675726C"_s }));
    EXPECT_EQ(0, pasteboard.addTypes({ "dyn.ah62d4rv4gu8y6y4grf0gn5xbrzw1gydcr7u1e3cytf2gn"_s }));

    // Non-file types should succeed. Declare ownership of the pasteboard first (as the real
    // write paths do via setTypes) so that addTypes reports a non-zero change count.
    pasteboard.setTypes({ "public.utf8-plain-text"_s });
    EXPECT_NE(0, pasteboard.addTypes({ "public.utf8-plain-text"_s }));
}

TEST(PlatformPasteboard, SetStringForTypeRejectsFileURL)
{
    WebCore::PlatformPasteboard pasteboard("com.apple.WebKit.test.file-type-setstring"_s);

    // Declare the URL type.
    pasteboard.setTypes({ "Apple URL pasteboard type"_s });

    // Writing a file:// URL via NSURLPboardType must NOT place data under UTTypeFileURL.
    pasteboard.setStringForType("file:///etc/passwd"_s, "Apple URL pasteboard type"_s);

    Vector<String> types;
    pasteboard.getTypes(types);
    for (auto& type : types)
        EXPECT_FALSE(type == "public.file-url"_s);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
