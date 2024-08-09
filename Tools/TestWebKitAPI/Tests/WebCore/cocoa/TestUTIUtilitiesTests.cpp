/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WebCore/UTIUtilities.h"

#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(UTIUtilities, MIMETypeFromUTIForMailPsd)
{
    EXPECT_EQ(MIMETypeFromUTI("com.adobe.photoshop-image"_s), "image/vnd.adobe.photoshop"_s);
}

TEST(UTIUtilities, RequiredMIMETypesFromUTIForMailPsd)
{
    auto mimeTypes = RequiredMIMETypesFromUTI("com.adobe.photoshop-image"_s);
    EXPECT_EQ(mimeTypes.size(), 2u);
    EXPECT_TRUE(mimeTypes.contains("image/vnd.adobe.photoshop"_s));
    // If this fails, Mail may not be able to display psd images when editing.
    EXPECT_TRUE(mimeTypes.contains("application/x-photoshop"_s));
}

}; // namespace TestWebKitAPI
