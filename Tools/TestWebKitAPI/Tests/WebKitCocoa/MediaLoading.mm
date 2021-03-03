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
#import "TestUIDelegate.h"
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

const char* videoPlayTestHTML ="<script>"
    "function createVideoElement() {"
        "let video = document.createElement('video');"
        "video.addEventListener('error', ()=>{alert('error')});"
        "video.addEventListener('playing', ()=>{alert('playing')});"
        "video.src='video.mp4';"
        "video.autoplay=1;"
        "document.body.appendChild(video);"
    "}"
"</script>"
"<body onload='createVideoElement()'></body>";

static Vector<uint8_t> testVideoBytes()
{
    NSData *data = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"test" withExtension:@"mp4" subdirectory:@"TestWebKitAPI.resources"]];
    Vector<uint8_t> vector;
    vector.append(static_cast<const uint8_t*>(data.bytes), data.length);
    return vector;
}

static void runVideoTest(NSURLRequest *request, const char* expectedMessage)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    [webView loadRequest:request];
    EXPECT_WK_STREQ([webView _test_waitForAlert], expectedMessage);
}

TEST(MediaLoading, RangeRequestSynthesisWithContentLength)
{
    HTTPServer server({
        {"/", { videoPlayTestHTML }},
        {"/video.mp4", { testVideoBytes() }}
    });
    runVideoTest(server.request(), "playing");
    EXPECT_EQ(server.totalRequests(), 2u);
}

TEST(MediaLoading, RangeRequestSynthesisWithoutContentLength)
{
    size_t totalRequests { 0 };
    Function<void(Connection)> respondToRequests;
    respondToRequests = [&] (Connection connection) {
        connection.receiveHTTPRequest([&, connection] (Vector<char>&& request) {
            auto sendResponse = [&, connection] (HTTPResponse response, HTTPResponse::IncludeContentLength includeContentLength) {
                connection.send(response.serialize(includeContentLength), [&, connection] {
                    respondToRequests(connection);
                });
            };
            totalRequests++;
            auto path = HTTPServer::parsePath(request);
            if (path == "/")
                sendResponse({ videoPlayTestHTML }, HTTPResponse::IncludeContentLength::Yes);
            else if (path == "/video.mp4")
                sendResponse(testVideoBytes(), HTTPResponse::IncludeContentLength::No);
            else
                ASSERT(path.isNull());
        });
    };

    HTTPServer server([&](Connection connection) {
        respondToRequests(connection);
    });
    runVideoTest(server.request(), "error");
    EXPECT_EQ(totalRequests, 2u);
}

} // namespace TestWebKitAPI

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)
