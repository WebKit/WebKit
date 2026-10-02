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

#pragma once

#ifdef __cplusplus

#import "Helpers/cocoa/FindTestPage.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestNSBundleExtras.h"
#import <WebKit/WKWebViewConfiguration.h>
#import <wtf/text/MakeString.h>

namespace TestWebKitAPI::FindTestFixtures {

inline FrameSpec singleFrame(const String& body)
{
    return { .body = body };
}

inline FrameSpec sameOriginChild()
{
    return { .body = "<p>hello</p>"_s, .children = { { .body = "<p>hello</p>"_s } } };
}

inline FrameSpec crossOriginChild()
{
    return { .body = "<p>hello</p>"_s, .children = { { .host = "b.com"_s, .body = "<p>hello</p>"_s } } };
}

inline FrameSpec nestedABA()
{
    return {
        .body = "<p>hello</p>"_s,
        .children = { { .host = "b.com"_s, .body = "<p>hello</p>"_s, .children = { { .body = "<p>hello</p>"_s } } } },
    };
}

inline FrameSpec emptyChildFrames()
{
    return {
        .body = "<p>hello</p>"_s,
        .children = {
            { .host = "b.com"_s, .body = "<p>nothing here</p>"_s },
            { .host = "c.com"_s, .body = "<p>hello</p>"_s },
            { .host = "d.com"_s, .body = "<p>nothing here</p>"_s },
        },
    };
}

inline FrameSpec hiddenChildFrame()
{
    return { .body = "<p>hello</p>"_s, .children = { { .host = "b.com"_s, .body = "<p>hello</p>"_s, .iframeAttributes = "style='display: none'"_s } } };
}

inline FrameSpec matchOnlyInChild()
{
    return { .body = "<p>nothing here</p>"_s, .children = { { .host = "b.com"_s, .body = "<p>hello</p>"_s } } };
}

inline FrameSpec matchesInEveryFrame()
{
    return {
        .body = "<p>hello</p>"_s,
        .children = {
            { .host = "b.com"_s, .body = "<p>hello</p>"_s },
            { .host = "c.com"_s, .body = "<p>hello</p>"_s },
            { .host = "d.com"_s, .body = "<p>hello</p>"_s },
        },
    };
}

inline FrameSpec shadowDOM()
{
    return singleFrame("<div id='host'></div><script>host.attachShadow({ mode: 'open' }).innerHTML = '<p>hello</p>';</script>"_s);
}

inline FrameSpec textControls()
{
    return singleFrame("<input value='hello'><textarea>hello</textarea>"_s);
}

inline FrameSpec contentEditable()
{
    return singleFrame("<div contenteditable>hello</div>"_s);
}

inline FrameSpec closedDetails()
{
    return singleFrame("<details><summary>summary</summary>hello</details>"_s);
}

inline FrameSpec hiddenUntilFound()
{
    return singleFrame("<div hidden='until-found'>hello</div>"_s);
}

inline FrameSpec userSelectNone()
{
    return singleFrame("<p style='user-select: none'>hello</p>"_s);
}

inline FrameSpec crossOriginChildWithCaptionedVideo()
{
    return {
        .body = "<p>hello</p>"_s,
        .children = { {
            .host = "b.com"_s,
            .body = "<video src='/test.mp4'></video><script>"
                "const track = document.querySelector('video').addTextTrack('captions', 'English', 'en');"
                "track.mode = 'showing';"
                "track.addCue(new VTTCue(2, 3, 'hello'));"
                "</script>"_s,
        } },
    };
}

inline HTTPServer::ResponseMap testBundleResource(NSString *extension, ASCIILiteral mimeType)
{
    HTTPResponse response { { { "Content-Type"_s, mimeType } }, [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:extension]] };
    HTTPServer::ResponseMap resources;
    resources.add(makeString("/test."_s, String(extension)), WTF::move(response));
    return resources;
}

inline HTTPServer::ResponseMap captionedVideoResources()
{
    return testBundleResource(@"mp4", "video/mp4"_s);
}

inline FrameSpec crossOriginChildPDF()
{
    return { .body = "<iframe src='https://b.com/test.pdf' style='width: 600px; height: 500px'></iframe>"_s };
}

inline HTTPServer::ResponseMap pdfResources()
{
    return testBundleResource(@"pdf", "application/pdf"_s);
}

inline RetainPtr<WKWebViewConfiguration> findInVideoConfiguration()
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    setFeatureEnabled(configuration.get(), @"FindInVideoEnabled", true);
    return configuration;
}

} // namespace TestWebKitAPI::FindTestFixtures

#endif // __cplusplus
