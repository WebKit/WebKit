/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "PlatformUtilities.h"
#include "WebCoreTestSupport.h"
#include <WebCore/ParserContentPolicy.h>
#include <libxml/parser.h>

namespace TestWebKitAPI {

TEST(XMLParsing, WebCoreDoesNotBreakLibxml2)
{
    // Parse XHTML in WebKit.
    String chunk = "<x/>"_s;
    auto result = WebCoreTestSupport::testDocumentFragmentParseXML(chunk, WebCore::DefaultParserContentPolicy);
    EXPECT_TRUE(result);

    NSString *resourceURL = [[[NSBundle mainBundle] URLForResource:@"test" withExtension:@"xml" subdirectory:@"TestWebKitAPI.resources"] path];

    // Parse an XML document in libxml2.
    xmlParserCtxtPtr ctxt = xmlNewParserCtxt();
    EXPECT_TRUE(!!ctxt);
    xmlDocPtr doc = xmlCtxtReadFile(ctxt, resourceURL.UTF8String, nullptr, XML_PARSE_NONET);
    EXPECT_TRUE(!!doc);
    xmlFreeDoc(doc);
    xmlFreeParserCtxt(ctxt);
}

} // namespace TestWebKitAPI
