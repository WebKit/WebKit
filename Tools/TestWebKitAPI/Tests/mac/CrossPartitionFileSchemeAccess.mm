/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

const char* successfulResult = "Pass: A cross partition resource was blocked from loading";

@interface CrossPartitionFileSchemeAccessNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation CrossPartitionFileSchemeAccessNavigationDelegate

static bool navigationComplete = false;

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    [webView evaluateJavaScript: @"document.body.innerHTML" completionHandler:^(NSString *result, NSError *error)
    {
        EXPECT_STREQ(successfulResult, [result UTF8String]);
        navigationComplete = true;
    }];
}

@end

void createPartition(const char *filePath) 
{
    const char* fileContent = " \"<!DOCTYPE html><html><body>Hello</body></html>\" > ";
    const char* targetFile = "resources/CrossPartitionFileSchemeAccess.html";
    
    const char* createDirCmd  = "mkdir resources";
    const char* createDiskImage = "hdiutil create otherVolume.dmg -srcfolder resources/ -ov > /dev/null";
    const char* attachDiskImage = "hdiutil attach otherVolume.dmg > /dev/null";
    
    std::string createFileCmd = "echo ";
    createFileCmd.append(fileContent);
    createFileCmd.append(targetFile);
    
    system(createDirCmd);
    system(createFileCmd.c_str());
    system(createDiskImage);
    system(attachDiskImage);
}

void cleanUp()
{
    const char* detachDiskImage = "hdiutil detach /Volumes/resources > /dev/null";
    const char* deleteFolder = "rm -rf resources/";
    const char* deleteDiskImage = "rm -rf otherVolume.dmg";
    system(detachDiskImage);
    system(deleteFolder);
    system(deleteDiskImage);
}


namespace TestWebKitAPI {

TEST(WebKitLegacy, CrossPartitionFileSchemeAccess)
{
    NSURL *url = [[NSBundle mainBundle] URLForResource:@"CrossPartitionFileSchemeAccess" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    const char *filePath = [url fileSystemRepresentation];
    createPartition(filePath);
        
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    
    CrossPartitionFileSchemeAccessNavigationDelegate *delegate = [[CrossPartitionFileSchemeAccessNavigationDelegate alloc] init];
    [webView setNavigationDelegate:delegate];
    
    NSURLRequest *request = [NSURLRequest requestWithURL:url];
    [webView loadRequest:request];
    Util::run(&navigationComplete);
    cleanUp();
}
}
