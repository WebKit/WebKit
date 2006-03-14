/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/DOMExtensions.h>
#import <WebKit/DOMRange.h>
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebCoreStatistics.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebEditingDelegate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebView.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebPluginDatabase.h>

#import <Carbon/Carbon.h>                           // for GetCurrentEventTime()
#import <ApplicationServices/ApplicationServices.h> // for CMSetDefaultProfileBySpace
#import <objc/objc-runtime.h>                       // for class_poseAs

#define COMMON_DIGEST_FOR_OPENSSL
#import <CommonCrypto/CommonDigest.h>               // for MD5 functions

#import <getopt.h>
#import <malloc/malloc.h>

#import "TextInputController.h"
#import "NavigationController.h"
#import "AppleScriptController.h"

@interface DumpRenderTreeWindow : NSWindow
@end

@interface DumpRenderTreeDraggingInfo : NSObject <NSDraggingInfo>
{
@private
    NSSize offset;
    NSImage *draggedImage;
    NSPasteboard *draggingPasteboard;
    id draggingSource;
}
- (id)initWithImage:(NSImage *)image offset:(NSSize)offset pasteboard:(NSPasteboard *)pasteboard source:(id)source; 
- (NSWindow *)draggingDestinationWindow;
- (NSDragOperation)draggingSourceOperationMask;
- (NSPoint)draggingLocation;
- (NSPoint)draggedImageLocation;
- (NSImage *)draggedImage;
- (NSPasteboard *)draggingPasteboard;
- (id)draggingSource;
- (int)draggingSequenceNumber;
- (void)slideDraggedImageTo:(NSPoint)screenPoint;
- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination;
@end

@interface DumpRenderTreePasteboard : NSPasteboard
@end

@interface WaitUntilDoneDelegate : NSObject
@end

@interface EditingDelegate : NSObject
@end

@interface LayoutTestController : NSObject
@end

@interface EventSendingController : NSObject
{
    BOOL down;
    int clickCount;
    NSTimeInterval lastClick;
    int eventNumber;
    double timeOffset;
}
@end

static void dumpRenderTree(const char *pathOrURL);
static NSString *md5HashStringForBitmap(CGImageRef bitmap);

static volatile BOOL done;
static WebFrame *frame;
static NavigationController *navigationController;
static BOOL readyToDump;
static BOOL waitToDump;
static BOOL dumpAsText;
static BOOL dumpTitleChanges;
static int dumpPixels = NO;
static int dumpTree = YES;
static BOOL printSeparators;
static NSString *currentTest = nil;
static NSPasteboard *localPasteboard;
static BOOL windowIsKey = YES;
static NSPoint lastMousePosition;
static DumpRenderTreeDraggingInfo *draggingInfo;
static unsigned char* screenCaptureBuffer;
static CGColorSpaceRef sharedColorSpace;

const unsigned maxViewHeight = 600;
const unsigned maxViewWidth = 800;

static CMProfileRef currentColorProfile = 0;
static void restoreColorSpace(int ignored)
{
    if (currentColorProfile) {
        int error = CMSetDefaultProfileByUse(cmDisplayUse, currentColorProfile);
        if (error)
            fprintf(stderr, "Failed to retore previous color profile!  You may need to open System Preferences : Displays : Color and manually restore your color settings.  (Error: %i)", error);
        currentColorProfile = 0;
    }
}

static void setDefaultColorProfileToRGB(void)
{
    CMProfileRef genericProfile = [[NSColorSpace genericRGBColorSpace] colorSyncProfile];
    CMProfileRef previousProfile;
    int error = CMGetDefaultProfileByUse(cmDisplayUse, &previousProfile);
    if (error) {
        fprintf(stderr, "Failed to get current color profile.  I will not be able to restore your current profile, thus I'm not changing it.  Many pixel tests may fail as a result.  (Error: %i)\n", error);
        return;
    }
    if (previousProfile == genericProfile)
        return;
    CFStringRef previousProfileName;
    CFStringRef genericProfileName;
    CMCopyProfileDescriptionString(previousProfile, &previousProfileName);
    CMCopyProfileDescriptionString(genericProfile, &genericProfileName);
    
    fprintf(stderr, "\n\nWARNING: Temporarily changing your system color profile from \"%s\" to \"%s\".\n",
        CFStringGetCStringPtr(previousProfileName, kCFStringEncodingMacRoman),
        CFStringGetCStringPtr(genericProfileName, kCFStringEncodingMacRoman));
    fprintf(stderr, "This allows the WebKit pixel-based regression tests to have consistent color values across all machines.\n");
    fprintf(stderr, "The colors on your screen will change for the duration of the testing.\n\n");
    
    if ((error = CMSetDefaultProfileByUse(cmDisplayUse, genericProfile)))
        fprintf(stderr, "Failed to set color profile to \"%s\"! Many pixel tests will fail as a result.  (Error: %i)",
            CFStringGetCStringPtr(genericProfileName, kCFStringEncodingMacRoman), error);
    else {
        currentColorProfile = previousProfile;
        signal(SIGINT, restoreColorSpace);
        signal(SIGHUP, restoreColorSpace);
        signal(SIGTERM, restoreColorSpace);
    }
    CFRelease(genericProfileName);
    CFRelease(previousProfileName);
}

static void* (*savedMalloc)(malloc_zone_t*, size_t);
static void* (*savedRealloc)(malloc_zone_t*, void*, size_t);

static void* checkedMalloc(malloc_zone_t* zone, size_t size)
{
    if (size >= 0x10000000)
        return 0;
    return savedMalloc(zone, size);
}

static void* checkedRealloc(malloc_zone_t* zone, void* ptr, size_t size)
{
    if (size >= 0x10000000)
        return 0;
    return savedRealloc(zone, ptr, size);
}

static void makeLargeMallocFailSilently(void)
{
    malloc_zone_t* zone = malloc_default_zone();
    savedMalloc = zone->malloc;
    savedRealloc = zone->realloc;
    zone->malloc = checkedMalloc;
    zone->realloc = checkedRealloc;
}

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    [NSApplication sharedApplication];

    class_poseAs(objc_getClass("DumpRenderTreePasteboard"), objc_getClass("NSPasteboard"));
    class_poseAs(objc_getClass("DumpRenderTreeWindow"), objc_getClass("NSWindow"));
    
    struct option options[] = {
        {"pixel-tests", no_argument, &dumpPixels, YES},
        {"tree", no_argument, &dumpTree, YES},
        {"notree", no_argument, &dumpTree, NO},
        {NULL, 0, NULL, 0}
    };

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:@"DoubleMax" forKey:@"AppleScrollBarVariant"];
    [defaults setInteger:4 forKey:@"AppleAntiAliasingThreshold"];
    // 2 is the "Medium" font smoothing mode
    [defaults setInteger:2 forKey:@"AppleFontSmoothing"];

    [defaults setInteger:1 forKey:@"AppleAquaColorVariant"];
    [defaults setObject:@"0.709800 0.835300 1.000000" forKey:@"AppleHighlightColor"];
    [defaults setObject:@"0.500000 0.500000 0.500000" forKey:@"AppleOtherHighlightColor"];
    
    [defaults setObject:[NSArray arrayWithObject:@"en"] forKey:@"AppleLanguages"];
    
    WebPreferences *preferences = [WebPreferences standardPreferences];
    
    [preferences setStandardFontFamily:@"Times"];
    [preferences setFixedFontFamily:@"Courier"];
    [preferences setSerifFontFamily:@"Times"];
    [preferences setSansSerifFontFamily:@"Helvetica"];
    [preferences setCursiveFontFamily:@"Apple Chancery"];
    [preferences setFantasyFontFamily:@"Papyrus"];
    [preferences setDefaultFontSize:16];
    [preferences setDefaultFixedFontSize:13];
    [preferences setMinimumFontSize:9];
    [preferences setJavaEnabled:NO];

    int option;
    while ((option = getopt_long(argc, (char * const *)argv, "", options, NULL)) != -1)
        switch (option) {
            case '?':   // unknown or ambiguous option
            case ':':   // missing argument
                exit(1);
                break;
        }
    
    if ([[[NSFontManager sharedFontManager] availableMembersOfFontFamily:@"Ahem"] count] == 0) {
        fprintf(stderr, "\nAhem font is not available. This special simple font is used to construct certain types of predictable tests.\n\nTo run regression tests, please get it from <http://webkit.opendarwin.org/quality/Ahem.ttf>.\n");
        exit(1);
    }
    
    if (dumpPixels) {
        setDefaultColorProfileToRGB();
        screenCaptureBuffer = malloc(maxViewHeight * maxViewWidth * 4);
        sharedColorSpace = CGColorSpaceCreateDeviceRGB();
    }
    
    localPasteboard = [NSPasteboard pasteboardWithUniqueName];
    navigationController = [[NavigationController alloc] init];

    NSRect rect = NSMakeRect(0, 0, maxViewWidth, maxViewHeight);
    
    WebView *webView = [[WebView alloc] initWithFrame:rect];
    WaitUntilDoneDelegate *delegate = [[WaitUntilDoneDelegate alloc] init];
    EditingDelegate *editingDelegate = [[EditingDelegate alloc] init];
    [webView setFrameLoadDelegate:delegate];
    [webView setEditingDelegate:editingDelegate];
    [webView setUIDelegate:delegate];
    frame = [webView mainFrame];
    
    NSString *pwd = [[NSString stringWithUTF8String:argv[0]] stringByDeletingLastPathComponent];
    [WebPluginDatabase setAdditionalWebPlugInPaths:[NSArray arrayWithObject:pwd]];
    [[WebPluginDatabase installedPlugins] refresh];

    // The back/forward cache is causing problems due to layouts during transition from one page to another.
    // So, turn it off for now, but we might want to turn it back on some day.
    [[webView backForwardList] setPageCacheSize:0];

    // To make things like certain NSViews, dragging, and plug-ins work, put the WebView a window, but put it off-screen so you don't see it.
    NSRect windowRect = NSOffsetRect(rect, -10000, -10000);
    NSWindow *window = [[NSWindow alloc] initWithContentRect:windowRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
    [[window contentView] addSubview:webView];
    [window orderBack:nil];
    [window setAutodisplay:NO];

    [webView setContinuousSpellCheckingEnabled:YES];

    makeLargeMallocFailSilently();
    
    // For reasons that are not entirely clear, the following pair of calls makes WebView handle its
    // dynamic scrollbars properly. Without it, every frame will always have scrollbars.
    NSBitmapImageRep *imageRep = [webView bitmapImageRepForCachingDisplayInRect:[webView bounds]];
    [webView cacheDisplayInRect:[webView bounds] toBitmapImageRep:imageRep];
    
    if (argc == optind+1 && strcmp(argv[optind], "-") == 0) {
        char filenameBuffer[2048];
        printSeparators = YES;
        while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
            char *newLineCharacter = strchr(filenameBuffer, '\n');
            if (newLineCharacter)
                *newLineCharacter = '\0';
            
            if (strlen(filenameBuffer) == 0)
                continue;
                
            dumpRenderTree(filenameBuffer);
            fflush(stdout);
        }
    } else {
        printSeparators = (optind < argc-1 || (dumpPixels && dumpTree));
        for (int i = optind; i != argc; ++i) {
            dumpRenderTree(argv[i]);
        }
    }
    
    [webView setFrameLoadDelegate:nil];
    [webView setEditingDelegate:nil];
    [webView setUIDelegate:nil];
    frame = nil;

    // Work around problem where registering drag types leaves an outstanding
    // "perform selector" on the window, which retains the window. It's a bit
    // inelegant and perhaps dangerous to just blow them all away, but in practice
    // it probably won't cause any trouble (and this is just a test tool, after all).
    [NSObject cancelPreviousPerformRequestsWithTarget:window];
    
    [window release];
    [webView release];
    [delegate release];
    [editingDelegate release];

    [localPasteboard releaseGlobally];
    localPasteboard = nil;
    
    [navigationController release];
    navigationController = nil;
    
    if (dumpPixels)
        restoreColorSpace(0);
    
    [pool release];

    return 0;
}

static void dump(void)
{
    NSString *result = nil;
    if (dumpTree) {
        if (dumpAsText) {
            DOMElement *documentElement = [[frame DOMDocument] documentElement];
            if ([documentElement isKindOfClass:[DOMHTMLElement class]])
                result = [[(DOMHTMLElement *)documentElement innerText] stringByAppendingString:@"\n"];
        } else {
            bool isSVGW3CTest = ([currentTest rangeOfString:@"svg/W3C-SVG-1.1"].length);
            if (isSVGW3CTest)
                [[frame webView] setFrameSize:NSMakeSize(480, 360)];
            else 
                [[frame webView] setFrameSize:NSMakeSize(maxViewWidth, maxViewHeight)];
            result = [frame renderTreeAsExternalRepresentation];
        }
        
        if (!result)
            printf("ERROR: nil result from %s", dumpAsText ? "[documentElement innerText]" : "[frame renderTreeAsExternalRepresentation]");
        else
            fputs([result UTF8String], stdout);
        
        if (printSeparators)
            puts("#EOF");
    }
    
    if (dumpPixels) {
        if (!dumpAsText) {
            // FIXME: It's unfortunate that we hardcode the file naming scheme here.
            // At one time, the perl script had all the knowledge about file layout.
            // Some day we should restore that setup by passing in more parameters to this tool.

            NSString *baseTestPath = [currentTest stringByDeletingPathExtension];
            NSString *baselineHashPath = [baseTestPath stringByAppendingString:@"-expected.checksum"];
            NSString *baselineHash = [NSString stringWithContentsOfFile:baselineHashPath encoding:NSUTF8StringEncoding error:nil];
            NSString *baselineImagePath = [baseTestPath stringByAppendingString:@"-expected.png"];
            
            // grab a bitmap from the view
            WebView *view = [frame webView];
            NSSize webViewSize = [[frame webView] frame].size;
            CGContextRef cgContext = CGBitmapContextCreate(screenCaptureBuffer, webViewSize.width, webViewSize.height, 8, webViewSize.width * 4, sharedColorSpace, kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedLast);
            NSGraphicsContext *nsContext = [NSGraphicsContext graphicsContextWithGraphicsPort:cgContext flipped:NO];
            [NSGraphicsContext saveGraphicsState];
            [NSGraphicsContext setCurrentContext:nsContext];
            [view displayRectIgnoringOpacity:NSMakeRect(0, 0, webViewSize.width, webViewSize.height) inContext:nsContext];
            [NSGraphicsContext restoreGraphicsState];
            
            // has the actual hash to compare to the expected image's hash
            CGImageRef bitmapImage = CGBitmapContextCreateImage(cgContext);
            NSString *actualHash = md5HashStringForBitmap(bitmapImage);
            printf("\nActualHash: %s\n", [actualHash UTF8String]);
            printf("BaselineHash: %s\n", [baselineHash UTF8String]);
            
            // if the hashes don't match, send image back to stdout for diff comparision
            if (![baselineHash isEqualToString:actualHash] || access([baselineImagePath fileSystemRepresentation], F_OK) != 0) {
                CFMutableDataRef imageData = CFDataCreateMutable(0, 0);
                CGImageDestinationRef imageDest = CGImageDestinationCreateWithData(imageData, CFSTR("public.png"), 1, 0);
                CGImageDestinationAddImage(imageDest, bitmapImage, 0);
                CGImageDestinationFinalize(imageDest);
                CFRelease(imageDest);
                printf("Content-length: %lu\n", CFDataGetLength(imageData));
                fwrite(CFDataGetBytePtr(imageData), 1, CFDataGetLength(imageData), stdout);
                CFRelease(imageData);
            }
            CGImageRelease(bitmapImage);
            CGContextRelease(cgContext);
        }

        printf("#EOF\n");
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

    windowIsKey = YES;
    NSView *documentView = [[frame frameView] documentView];
    [[[frame webView] window] makeFirstResponder:documentView];
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)documentView _updateFocusState];
}

- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    [self webView:sender locationChangeDone:error forDataSource:[frame provisionalDataSource]];
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    [self webView:sender locationChangeDone:nil forDataSource:[frame dataSource]];
    [navigationController webView:sender didFinishLoadForFrame:frame];
}

- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    [self webView:sender locationChangeDone:error forDataSource:[frame dataSource]];
}

- (void)webView:(WebView *)sender windowScriptObjectAvailable:(WebScriptObject *)obj 
{ 
    LayoutTestController *ltc = [[LayoutTestController alloc] init];
    [obj setValue:ltc forKey:@"layoutTestController"];
    [ltc release];
    EventSendingController *esc = [[EventSendingController alloc] init];
    [obj setValue:esc forKey:@"eventSender"];
    [esc release];
    TextInputController *tic = [[TextInputController alloc] initWithWebView:sender];
    [obj setValue:tic forKey:@"textInputController"];
    [tic release];
    AppleScriptController *asc = [[AppleScriptController alloc] initWithWebView:sender];
    [obj setValue:asc forKey:@"appleScriptController"];
    [asc release];
    [obj setValue:navigationController forKey:@"navigationController"];
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

- (void)webView:(WebView *)sender dragImage:(NSImage *)anImage at:(NSPoint)viewLocation offset:(NSSize)initialOffset event:(NSEvent *)event pasteboard:(NSPasteboard *)pboard source:(id)sourceObj slideBack:(BOOL)slideFlag forView:(NSView *)view
{
    // A new drag was started before the old one ended.  Probably shouldn't happen.
    if (draggingInfo) {
        [[draggingInfo draggingSource] draggedImage:[draggingInfo draggedImage] endedAt:lastMousePosition operation:NSDragOperationNone];
        [draggingInfo release];
    }
    draggingInfo = [[DumpRenderTreeDraggingInfo alloc] initWithImage:anImage offset:initialOffset pasteboard:pboard source:sourceObj];
}

- (void)webViewFocus:(WebView *)webView
{
    windowIsKey = YES;
    NSView *documentView = [[frame frameView] documentView];
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)documentView _updateFocusState];
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
            || aSelector == @selector(dumpTitleChanges)
            || aSelector == @selector(setWindowIsKey:)
            || aSelector == @selector(setMainFrameIsFirstResponder:))
        return NO;
    return YES;
}

+ (NSString *)webScriptNameForSelector:(SEL)aSelector
{
    if (aSelector == @selector(setWindowIsKey:))
        return @"setWindowIsKey";
    if (aSelector == @selector(setMainFrameIsFirstResponder:))
        return @"setMainFrameIsFirstResponder";
    return nil;
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

- (void)setWindowIsKey:(BOOL)flag
{
    windowIsKey = flag;
    NSView *documentView = [[frame frameView] documentView];
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)documentView _updateFocusState];
}

- (void)setMainFrameIsFirstResponder:(BOOL)flag
{
    NSView *documentView = [[frame frameView] documentView];
    
    NSResponder *firstResponder = flag ? documentView : nil;
    [[[frame webView] window] makeFirstResponder:firstResponder];
        
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)documentView _updateFocusState];
}

- (id)invokeUndefinedMethodFromWebScript:(NSString *)name withArguments:(NSArray *)args
{
    return nil;
}

@end

@implementation EventSendingController

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    if (aSelector == @selector(mouseDown)
            || aSelector == @selector(mouseUp)
            || aSelector == @selector(mouseClick)
            || aSelector == @selector(mouseMoveToX:Y:)
            || aSelector == @selector(leapForward:))
        return NO;
    return YES;
}

+ (NSString *)webScriptNameForSelector:(SEL)aSelector
{
    if (aSelector == @selector(mouseMoveToX:Y:))
        return @"mouseMoveTo";
    if (aSelector == @selector(leapForward:))
        return @"leapForward";
    return nil;
}

- (id)init
{
    lastMousePosition = NSMakePoint(0, 0);
    down = NO;
    clickCount = 0;
    lastClick = 0;
    return self;
}

- (double)currentEventTime
{
    return GetCurrentEventTime() + timeOffset;
}

- (void)leapForward:(int)milliseconds
{
    timeOffset += milliseconds / 1000.0;
}

- (void)mouseDown
{
    [[[frame frameView] documentView] layout];
    if ([self currentEventTime] - lastClick >= 1)
        clickCount = 1;
    else
        clickCount++;
    NSEvent *event = [NSEvent mouseEventWithType:NSLeftMouseDown 
                                        location:lastMousePosition 
                                   modifierFlags:nil 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[frame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:clickCount 
                                        pressure:nil];

    NSView *subView = [[frame webView] hitTest:[event locationInWindow]];
    if (subView) {
        [subView mouseDown:event];
        down = YES;
    }
}

- (void)mouseUp
{
    [[[frame frameView] documentView] layout];
    NSEvent *event = [NSEvent mouseEventWithType:NSLeftMouseUp 
                                        location:lastMousePosition 
                                   modifierFlags:nil 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[frame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:clickCount 
                                        pressure:nil];

    NSView *subView = [[frame webView] hitTest:[event locationInWindow]];
    if (subView) {
        [subView mouseUp:event];
        down = NO;
        lastClick = [event timestamp];
        if (draggingInfo) {
            WebView *webView = [frame webView];
            
            NSDragOperation dragOperation = [webView draggingUpdated:draggingInfo];
            
            [[draggingInfo draggingSource] draggedImage:[draggingInfo draggedImage] endedAt:lastMousePosition operation:dragOperation];
            if (dragOperation != NSDragOperationNone)
                [webView performDragOperation:draggingInfo];
            [draggingInfo release];
            draggingInfo = nil;
        }
    }
}

- (void)mouseMoveToX:(int)x Y:(int)y
{
    lastMousePosition = NSMakePoint(x, [[frame webView] frame].size.height - y);
    NSEvent *event = [NSEvent mouseEventWithType:(down ? NSLeftMouseDragged : NSMouseMoved) 
                                        location:lastMousePosition 
                                   modifierFlags:nil 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[frame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:(down ? clickCount : 0) 
                                        pressure:nil];

    NSView *subView = [[frame webView] hitTest:[event locationInWindow]];
    if (subView) {
        if (down) {
            [subView mouseDragged:event];
            [[draggingInfo draggingSource] draggedImage:[draggingInfo draggedImage] movedTo:lastMousePosition];
            [[frame webView] draggingUpdated:draggingInfo];
        }
        else
            [subView mouseMoved:event];
    }
}

- (void)mouseClick
{
    [[[frame frameView] documentView] layout];
    if ([self currentEventTime] - lastClick >= 1)
        clickCount = 1;
    else
        clickCount++;
    NSEvent *mouseDownEvent = [NSEvent mouseEventWithType:NSLeftMouseDown 
                                        location:lastMousePosition 
                                   modifierFlags:nil 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[frame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:clickCount 
                                        pressure:nil];

    NSView *subView = [[frame webView] hitTest:[mouseDownEvent locationInWindow]];
    if (subView) {
        [self leapForward:1];
        NSEvent *mouseUpEvent = [NSEvent mouseEventWithType:NSLeftMouseUp
                                                   location:lastMousePosition
                                              modifierFlags:nil
                                                  timestamp:[self currentEventTime]
                                               windowNumber:[[[frame webView] window] windowNumber]
                                                    context:[NSGraphicsContext currentContext]
                                                eventNumber:++eventNumber
                                                 clickCount:clickCount
                                                   pressure:nil];
        [NSApp postEvent:mouseUpEvent atStart:NO];
        [subView mouseDown:mouseDownEvent];
    }
}

@end

static void dumpRenderTree(const char *pathOrURL)
{
    CFStringRef pathOrURLString = CFStringCreateWithCString(NULL, pathOrURL, kCFStringEncodingUTF8);
    if (!pathOrURLString) {
        fprintf(stderr, "can't parse filename as UTF-8\n");
        return;
    }

    CFURLRef URL;
    if (CFStringHasPrefix(pathOrURLString, CFSTR("http://")))
        URL = CFURLCreateWithString(NULL, pathOrURLString, NULL);
    else
        URL = CFURLCreateWithFileSystemPath(NULL, pathOrURLString, kCFURLPOSIXPathStyle, FALSE);
    
    if (!URL) {
        CFRelease(pathOrURLString);
        fprintf(stderr, "can't turn %s into a CFURL\n", pathOrURL);
        return;
    }

    done = NO;
    readyToDump = NO;
    waitToDump = NO;
    dumpAsText = NO;
    dumpTitleChanges = NO;
    if (currentTest != nil)
        CFRelease(currentTest);
    currentTest = (NSString *)pathOrURLString;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [frame loadRequest:[NSURLRequest requestWithURL:(NSURL *)URL]];
    CFRelease(URL);
    [pool release];
    while (!done) {
        pool = [[NSAutoreleasePool alloc] init];
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
        [pool release];
    }
    pool = [[NSAutoreleasePool alloc] init];
    [[frame webView] setSelectedDOMRange:nil affinity:NSSelectionAffinityDownstream];
    if (draggingInfo)
        [draggingInfo release];
    draggingInfo = nil;
    [pool release];
}

/* Hashes a bitmap and returns a text string for comparison and saving to a file */
static NSString *md5HashStringForBitmap(CGImageRef bitmap)
{
    MD5_CTX md5Context;
    unsigned char hash[16];
    
    unsigned bitsPerPixel = CGImageGetBitsPerPixel(bitmap);
    assert(bitsPerPixel == 32); // ImageDiff assumes 32 bit RGBA, we must as well.
    unsigned bytesPerPixel = bitsPerPixel / 8;
    unsigned pixelsHigh = CGImageGetHeight(bitmap);
    unsigned pixelsWide = CGImageGetWidth(bitmap);
    unsigned bytesPerRow = CGImageGetBytesPerRow(bitmap);
    assert(bytesPerRow >= (pixelsWide * bytesPerPixel));
    
    MD5_Init(&md5Context);
    unsigned char *bitmapData = screenCaptureBuffer;
    for (unsigned row = 0; row < pixelsHigh; row++) {
        MD5_Update(&md5Context, bitmapData, pixelsWide * bytesPerPixel);
        bitmapData += bytesPerRow;
    }
    MD5_Final(hash, &md5Context);
    
    char hex[33] = "";
    for (int i = 0; i < 16; i++) {
       snprintf(hex, 33, "%s%02x", hex, hash[i]);
    }

    return [NSString stringWithUTF8String:hex];
}

@implementation DumpRenderTreePasteboard

// Return a local pasteboard so we don't disturb the real pasteboard when running tests.
+ (NSPasteboard *)generalPasteboard
{
    return localPasteboard;
}

@end

@implementation DumpRenderTreeWindow

- (BOOL)isKeyWindow
{
    return windowIsKey;
}

@end

@implementation DumpRenderTreeDraggingInfo

- (id)initWithImage:(NSImage *)anImage offset:(NSSize)o pasteboard:(NSPasteboard *)pboard source:(id)source
{
    draggedImage = [anImage retain];
    draggingPasteboard = [pboard retain];
    draggingSource = [source retain];
    offset = o;
    
    return [super init];
}

- (void)dealloc
{
    [draggedImage release];
    [draggingPasteboard release];
    [draggingSource release];
    [super dealloc];
}

- (NSWindow *)draggingDestinationWindow 
{
    return [[frame webView] window];
}

- (NSDragOperation)draggingSourceOperationMask 
{
    return [draggingSource draggingSourceOperationMaskForLocal:YES];
}

- (NSPoint)draggingLocation
{ 
    return lastMousePosition; 
}

- (NSPoint)draggedImageLocation 
{
    return NSMakePoint(lastMousePosition.x + offset.width, lastMousePosition.y + offset.height);
}

- (NSImage *)draggedImage
{
    return draggedImage;
}

- (NSPasteboard *)draggingPasteboard
{
    return draggingPasteboard;
}

- (id)draggingSource
{
    return draggingSource;
}

- (int)draggingSequenceNumber
{
    NSLog(@"DumpRenderTree doesn't support draggingSequenceNumber");
    return 0;
}

- (void)slideDraggedImageTo:(NSPoint)screenPoint
{
    NSLog(@"DumpRenderTree doesn't support slideDraggedImageTo:");
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    NSLog(@"DumpRenderTree doesn't support namesOfPromisedFilesDroppedAtDestination:");
    return nil;
}

@end

