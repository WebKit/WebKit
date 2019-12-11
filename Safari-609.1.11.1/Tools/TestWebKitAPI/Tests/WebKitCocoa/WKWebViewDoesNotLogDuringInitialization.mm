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

#import "config.h"

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <fcntl.h>

TEST(WKWebView, InitializingWebViewWithEphemeralStorageDoesNotLog)
{
    // Replace stderr with a nonblocking pipe that we can read from.
    int p[2];
    pipe(p);
    fcntl(p[0], F_SETFL, fcntl(p[0], F_GETFL, 0) | O_NONBLOCK);
    dup2(p[1], STDERR_FILENO);
    close(p[1]);

    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    _WKProcessPoolConfiguration *processPoolConfiguration = [[_WKProcessPoolConfiguration alloc] init];
    configuration.processPool = [[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration];
    configuration.websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];

    [[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration];

    FILE *stderrFileHandle = fdopen(p[0], "r");
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), stderrFileHandle)) {
        printf("Saw unexpected logging: '%s'\n", buffer);
        FAIL();
    }
}
