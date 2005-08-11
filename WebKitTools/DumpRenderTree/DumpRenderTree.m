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
#import <WebKit/DOMRange.h>
#import <WebKit/WebCoreStatistics.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebEditingDelegate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebView.h>

#import <getopt.h>

@interface WaitUntilDoneDelegate : NSObject
@end

@interface EditingDelegate : NSObject
@end

@interface LayoutTestController : NSObject
@end

static void dumpRenderTree(const char *filename);

static volatile BOOL done;
static WebFrame *frame;
static BOOL readyToDump;
static BOOL waitToDump;
static BOOL dumpAsText;
static BOOL dumpTitleChanges;
static int dumpPixels = NO;
static int dumpTree = YES;
static BOOL printSeparators;

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
    int width = 800;
    int height = 600;
    
    struct option options[] = {
        {"width", required_argument, NULL, 'w'},
        {"height", required_argument, NULL, 'h'},
        {"bitmap", no_argument, &dumpPixels, YES},
        {"nobitmap", no_argument, &dumpPixels, NO},
        {"tree", no_argument, &dumpTree, YES},
        {"notree", no_argument, &dumpTree, NO},
        {NULL, 0, NULL, 0}
    };
    int option;

    [preferences setStandardFontFamily:@"Times"];
    [preferences setFixedFontFamily:@"Courier"];
    [preferences setSerifFontFamily:@"Times"];
    [preferences setSansSerifFontFamily:@"Helvetica"];
    [preferences setCursiveFontFamily:@"Apple Chancery"];
    [preferences setFantasyFontFamily:@"Papyrus"];
    [preferences setDefaultFontSize:16];
    [preferences setDefaultFixedFontSize:13];
    [preferences setMinimumFontSize:9];

    while ((option = getopt_long(argc, (char * const *)argv, "", options, NULL)) != -1)
        switch (option) {
            case 'w':
                width = strtol(optarg, NULL, 0);
                if (width <= 0) {
                    fprintf(stderr, "%s: invalid width\n", argv[0]);
                    exit(1);
                }
                break;
            case 'h':
                height = strtol(optarg, NULL, 0);
                if (height <= 0) {
                    fprintf(stderr, "%s: invalid height\n", argv[0]);
                    exit(1);
                }
                break;
            case '?':   // unknown or ambiguous option
            case ':':   // missing argument
                exit(1);
                break;
        }
    
    WebView *webView = [[WebView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];
    WaitUntilDoneDelegate *delegate = [[WaitUntilDoneDelegate alloc] init];
    EditingDelegate *editingDelegate = [[EditingDelegate alloc] init];
    [webView setFrameLoadDelegate:delegate];
    [webView setEditingDelegate:editingDelegate];
    [webView setUIDelegate:delegate];
    frame = [webView mainFrame];
    
    if (argc == optind+1 && strcmp(argv[optind], "-") == 0) {
        char filenameBuffer[2048];
        printSeparators = YES;
        while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
            char *newLineCharacter = strchr(filenameBuffer, '\n');
            if (newLineCharacter) {
                *newLineCharacter = '\0';
            }
            dumpRenderTree(filenameBuffer);
            fflush(stdout);
        }
    } else {
        int i;
        printSeparators = (optind < argc-1 || (dumpPixels && dumpTree));
        for (i = optind; i != argc; ++i) {
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
    if (dumpTree) {
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
        if (printSeparators)
            puts("#EOF");
    }
    
    if (dumpPixels) {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        WebView *view = [frame webView];
        NSBitmapImageRep *imageRep = [view bitmapImageRepForCachingDisplayInRect:[view frame]];
        NSData *imageData;
        [view cacheDisplayInRect:[view frame] toBitmapImageRep:imageRep];
        imageData = [imageRep representationUsingType:NSPNGFileType properties:nil];
        if (printSeparators)
            printf("%d\n", [imageData length]);
        fwrite([imageData bytes], 1, [imageData length], stdout);
        [pool release];
    }

    done = YES;
}

@implementation WaitUntilDoneDelegate

- (void)webView:(WebView *)c locationChangeDone:(NSError *)error forDataSource:(WebDataSource *)dataSource
{
    if ([dataSource webFrame] == frame) {
        if (waitToDump)
            readyToDump = YES;
        else
            dump();
    }
}

- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)f
{
    if (frame == f)
        readyToDump = NO;
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

- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame
{
    if (dumpTitleChanges)
        printf("TITLE CHANGED: %s\n", [title UTF8String]);
}

@end

@interface DOMNode (dumpPath)
- (NSString *)dumpPath;
@end

@implementation DOMNode (dumpPath)
- (NSString *)dumpPath
{
    DOMNode *parent = [self parentNode];
    NSString *str = [NSString stringWithFormat:@"%@", [self nodeName]];
    if (parent != nil) {
        str = [str stringByAppendingString:@" > "];
        str = [str stringByAppendingString:[parent dumpPath]];
    }
    return str;
}
@end

@interface DOMRange (dump)
- (NSString *)dump;
@end

@implementation DOMRange (dump)
- (NSString *)dump
{
    return [NSString stringWithFormat:@"range from %ld of %@ to %ld of %@", [self startOffset], [[self startContainer] dumpPath], [self endOffset], [[self endContainer] dumpPath]];
}
@end


@implementation EditingDelegate

- (BOOL)webView:(WebView *)webView shouldBeginEditingInDOMRange:(DOMRange *)range
{
    printf("EDITING DELEGATE: shouldBeginEditingInDOMRange:%s\n", [[range dump] UTF8String]);
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldEndEditingInDOMRange:(DOMRange *)range
{
    printf("EDITING DELEGATE: shouldEndEditingInDOMRange:%s\n", [[range dump] UTF8String]);
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldInsertNode:(DOMNode *)node replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action
{
    static const char *insertactionstring[] = {
        "WebViewInsertActionTyped",
        "WebViewInsertActionPasted",
        "WebViewInsertActionDropped",
    };

    printf("EDITING DELEGATE: shouldInsertNode:%s replacingDOMRange:%s givenAction:%s\n", [[node dumpPath] UTF8String], [[range dump] UTF8String], insertactionstring[action]);
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldInsertText:(NSString *)text replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action
{
    static const char *insertactionstring[] = {
        "WebViewInsertActionTyped",
        "WebViewInsertActionPasted",
        "WebViewInsertActionDropped",
    };

    printf("EDITING DELEGATE: shouldInsertText:%s replacingDOMRange:%s givenAction:%s\n", [[text description] UTF8String], [[range dump] UTF8String], insertactionstring[action]);
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldDeleteDOMRange:(DOMRange *)range
{
    printf("EDITING DELEGATE: shouldDeleteDOMRange:%s\n", [[range dump] UTF8String]);
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldChangeSelectedDOMRange:(DOMRange *)currentRange toDOMRange:(DOMRange *)proposedRange affinity:(NSSelectionAffinity)selectionAffinity stillSelecting:(BOOL)flag
{
    static const char *affinitystring[] = {
        "NSSelectionAffinityUpstream",
        "NSSelectionAffinityDownstream"
    };
    static const char *boolstring[] = {
        "FALSE",
        "TRUE"
    };

    printf("EDITING DELEGATE: shouldChangeSelectedDOMRange:%s toDOMRange:%s affinity:%s stillSelecting:%s\n", [[currentRange dump] UTF8String], [[proposedRange dump] UTF8String], affinitystring[selectionAffinity], boolstring[flag]);
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldApplyStyle:(DOMCSSStyleDeclaration *)style toElementsInDOMRange:(DOMRange *)range
{
    printf("EDITING DELEGATE: shouldApplyStyle:%s toElementsInDOMRange:%s\n", [[style description] UTF8String], [[range dump] UTF8String]);
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldChangeTypingStyle:(DOMCSSStyleDeclaration *)currentStyle toStyle:(DOMCSSStyleDeclaration *)proposedStyle
{
    printf("EDITING DELEGATE: shouldChangeTypingStyle:%s toStyle:%s\n", [[currentStyle description] UTF8String], [[proposedStyle description] UTF8String]);
    return YES;
}

- (void)webViewDidBeginEditing:(NSNotification *)notification
{
    printf("EDITING DELEGATE: webViewDidBeginEditing:%s\n", [[notification name] UTF8String]);
}

- (void)webViewDidChange:(NSNotification *)notification
{
    printf("EDITING DELEGATE: webViewDidChange:%s\n", [[notification name] UTF8String]);
}

- (void)webViewDidEndEditing:(NSNotification *)notification
{
    printf("EDITING DELEGATE: webViewDidEndEditing:%s\n", [[notification name] UTF8String]);
}

- (void)webViewDidChangeTypingStyle:(NSNotification *)notification
{
    printf("EDITING DELEGATE: webViewDidChangeTypingStyle:%s\n", [[notification name] UTF8String]);
}

- (void)webViewDidChangeSelection:(NSNotification *)notification
{
    if (!done)
        printf("EDITING DELEGATE: webViewDidChangeSelection:%s\n", [[notification name] UTF8String]);
}

@end

@implementation LayoutTestController

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    if (aSelector == @selector(waitUntilDone)
            || aSelector == @selector(notifyDone)
            || aSelector == @selector(dumpAsText)
            || aSelector == @selector(dumpTitleChanges))
        return NO;
    return YES;
}

- (void)waitUntilDone 
{
    waitToDump = YES;
}

- (void)notifyDone
{
    if (waitToDump && readyToDump)
        dump();
    waitToDump = NO;
}

- (void)dumpAsText
{
    dumpAsText = YES;
}

- (void)dumpTitleChanges
{
    dumpTitleChanges = YES;
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
    readyToDump = NO;
    waitToDump = NO;
    dumpAsText = NO;
    dumpTitleChanges = NO;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [frame loadRequest:[NSURLRequest requestWithURL:(NSURL *)URL]];
    [pool release];
    while (!done) {
        pool = [[NSAutoreleasePool alloc] init];
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
        [pool release];
    }
    [[frame webView] setSelectedDOMRange:nil affinity:NSSelectionAffinityDownstream];
}
