/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Foundation/NSURLRequest.h>
#import <Foundation/NSError.h>

#import <WebKit/DOMExtensions.h>
#import <WebKit/WebCoreStatistics.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebView.h>

@interface WaitUntilDoneDelegate : NSObject
@end

@interface LayoutTestController : NSObject
@end

static void dumpRenderTree(const char *filename);

static volatile BOOL done;
static WebFrame *frame;
static BOOL waitLayoutTest;
static BOOL dumpAsText;

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    WebPreferences *preferences = [WebPreferences standardPreferences];
    
    NSString *standardFontFamily = [preferences standardFontFamily];
    NSString *fixedFontFamily = [preferences fixedFontFamily];
    NSString *serifFontFamily = [preferences serifFontFamily];
    NSString *sansSerifFontFamily = [preferences sansSerifFontFamily];
    NSString *cursiveFontFamily = [preferences cursiveFontFamily];
    NSString *fantasyFontFamily = [preferences fantasyFontFamily];
    int defaultFontSize = [preferences defaultFontSize];
    int defaultFixedFontSize = [preferences defaultFixedFontSize];
    int minimumFontSize = [preferences minimumFontSize];
    
    [preferences setStandardFontFamily:@"Times"];
    [preferences setFixedFontFamily:@"Courier"];
    [preferences setSerifFontFamily:@"Times"];
    [preferences setSansSerifFontFamily:@"Helvetica"];
    [preferences setCursiveFontFamily:@"Apple Chancery"];
    [preferences setFantasyFontFamily:@"Papyrus"];
    [preferences setDefaultFontSize:16];
    [preferences setDefaultFixedFontSize:13];
    [preferences setMinimumFontSize:9];

    WebView *webView = [[WebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)];
    WaitUntilDoneDelegate *delegate = [[WaitUntilDoneDelegate alloc] init];
    [webView setFrameLoadDelegate:delegate];
    [webView setUIDelegate:delegate];
    frame = [webView mainFrame];
    
    if (argc == 2 && strcmp(argv[1], "-") == 0) {
        char filenameBuffer[2048];
        while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
            char *newLineCharacter = strchr(filenameBuffer, '\n');
            if (newLineCharacter) {
                *newLineCharacter = '\0';
            }
            dumpRenderTree(filenameBuffer);
            puts("#EOF");
            fflush(stdout);
        }
    } else {
        int i;
        for (i = 1; i != argc; ++i) {
            dumpRenderTree(argv[i]);
        }
    }
    
    [preferences setStandardFontFamily:standardFontFamily];
    [preferences setFixedFontFamily:fixedFontFamily];
    [preferences setSerifFontFamily:serifFontFamily];
    [preferences setSansSerifFontFamily:sansSerifFontFamily];
    [preferences setCursiveFontFamily:cursiveFontFamily];
    [preferences setFantasyFontFamily:fantasyFontFamily];
    [preferences setDefaultFontSize:defaultFontSize];
    [preferences setDefaultFixedFontSize:defaultFixedFontSize];
    [preferences setMinimumFontSize:minimumFontSize];

    [pool release];
    return 0;
}

static void dump(void)
{
    NSString *result = nil;
    if (dumpAsText) {
        DOMDocument *document = [frame DOMDocument];
        if ([document isKindOfClass:[DOMHTMLDocument class]]) {
            result = [[[(DOMHTMLDocument *)document body] innerText] stringByAppendingString:@"\n"];
        }
    } else {
        result = [frame renderTreeAsExternalRepresentation];
    }
    if (!result) {
        puts("error");
    } else {
        fputs([result UTF8String], stdout);
    }
    done = YES;
}

@implementation WaitUntilDoneDelegate

- (void)webView:(WebView *)c locationChangeDone:(NSError *)error forDataSource:(WebDataSource *)dataSource
{
    if (!waitLayoutTest && [dataSource webFrame] == frame) {
        dump();
    }
}

- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    [self webView:sender locationChangeDone:error forDataSource:[frame provisionalDataSource]];
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    [self webView:sender locationChangeDone:nil forDataSource:[frame dataSource]];
}

- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    [self webView:sender locationChangeDone:error forDataSource:[frame dataSource]];
}

- (void)webView:(WebView *)sender windowScriptObjectAvailable:(WebScriptObject *)obj 
{ 
    LayoutTestController *ltc = [[LayoutTestController alloc] init];
    [(id)obj setValue:ltc forKey:@"layoutTestController"];
    [ltc release];

}

- (void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message
{
    printf("ALERT: %s\n", [message UTF8String]);
}

@end

@implementation LayoutTestController

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    if (aSelector == @selector(waitUntilDone) || aSelector == @selector(notifyDone) || aSelector == @selector(dumpAsText))
        return NO;
    return YES;
}

- (void)waitUntilDone 
{
    waitLayoutTest = YES;
}

- (void)notifyDone
{
    dump();
    waitLayoutTest = NO;
}

- (void)dumpAsText
{
    dumpAsText = YES;
}

@end

static void dumpRenderTree(const char *filename)
{
    CFStringRef filenameString = CFStringCreateWithCString(NULL, filename, kCFStringEncodingUTF8);
    if (filenameString == NULL) {
        fprintf(stderr, "can't parse filename as UTF-8\n");
        return;
    }
    CFURLRef URL = CFURLCreateWithFileSystemPath(NULL, filenameString, kCFURLPOSIXPathStyle, FALSE);
    if (URL == NULL) {
        fprintf(stderr, "can't turn %s into a CFURL\n", filename);
        return;
    }

    done = NO;
    dumpAsText = NO;
    waitLayoutTest = NO;

    [frame loadRequest:[NSURLRequest requestWithURL:(NSURL *)URL]];
    NSDate *date = [NSDate distantPast];
    while (!done) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:date];
    }
}
