/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <wtf/text/StringConcatenateNumbers.h>

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

namespace TestWebKitAPI {

static String parseUserAgent(const Vector<char>& request)
{
    auto headers = String::fromUTF8(request.data(), request.size()).split("\r\n");
    auto index = headers.findMatching([] (auto& header) { return header.startsWith("User-Agent:"); });
    if (index != notFound)
        return headers[index];
    return emptyString();
}

TEST(MediaLoading, UserAgentStringCRABS)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    webView.get().customUserAgent = @"TestWebKitAPI";

    bool receivedMediaRequest = false;

    HTTPServer server([&](Connection connection) mutable {
        connection.receiveHTTPRequest([&] (auto&& request) {
            auto userAgent = parseUserAgent(request);
            EXPECT_STREQ("User-Agent: TestWebKitAPI", userAgent.utf8().data());

            receivedMediaRequest = true;
        });
    });

    [webView loadHTMLString:[NSString stringWithFormat:@"<video src='http://127.0.0.1:%d/video.mp4' autoplay></video>", server.port()] baseURL:nil];

    Util::run(&receivedMediaRequest);
}

TEST(MediaLoading, UserAgentStringHLS)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    webView.get().customUserAgent = @"TestWebKitAPI";

    bool receivedManifestRequest = false;
    bool receivedMediaRequest = false;

    HTTPServer mediaServer([&](Connection connection) mutable {
        connection.receiveHTTPRequest([connection, &receivedMediaRequest] (Vector<char>&& request) {
            auto userAgent = parseUserAgent(request);
            EXPECT_STREQ("User-Agent: TestWebKitAPI", userAgent.utf8().data());
            receivedMediaRequest = true;
        });
    });
    auto mediaServerPort = mediaServer.port();

    HTTPServer manifestServer([&](Connection connection) mutable {
        connection.receiveHTTPRequest([connection, mediaServerPort, &receivedManifestRequest] (Vector<char>&& request) {
            auto userAgent = parseUserAgent(request);
            EXPECT_STREQ("User-Agent: TestWebKitAPI", userAgent.utf8().data());

            auto payload = makeString(
                "#EXTM3U\n"
                "#EXT-X-TARGETDURATION:6\n"
                "#EXT-X-VERSION:4\n"
                "#EXT-X-MEDIA-SEQUENCE:0\n"
                "#EXT-X-PLAYLIST-TYPE:VOD\n"
                "#EXTINF:6.0272,\n"
                "http://127.0.0.1:", mediaServerPort, "/main1.ts\n",
                "#EXT-X-ENDLIST\n"
            );

            connection.send(makeString("HTTP/1.1 200 OK\r\n",
                "Content-Length: ", payload.length(), "\r\n",
                "\r\n",
                payload
            ));

            receivedManifestRequest = true;
        });
    });

    [webView loadHTMLString:[NSString stringWithFormat:@"<video src='http://127.0.0.1:%d/manifest.m3u8' autoplay></video>", manifestServer.port()] baseURL:nil];

    Util::run(&receivedMediaRequest);
}

}

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)
