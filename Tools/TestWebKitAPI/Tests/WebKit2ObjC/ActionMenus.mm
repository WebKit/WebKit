/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "Test.h"

#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import "TestBrowsingContextLoadDelegate.h"
#import <JavaScriptCore/JSContextRef.h>
#import <JavaScriptCore/JSRetainPtr.h>
#import <WebKit/WebKit2.h>
#import <WebKit/WKActionMenuItemTypes.h>
#import <WebKit/WKActionMenuTypes.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKSerializedScriptValue.h>
#import <WebKit/WKViewPrivate.h>
#import <wtf/RetainPtr.h>

static bool didFinishLoad = false;
static bool didFinishDownload = false;

@interface WKView (Details)

- (void)prepareForMenu:(NSMenu *)menu withEvent:(NSEvent *)event;
- (void)willOpenMenu:(NSMenu *)menu withEvent:(NSEvent *)event;
- (void)didCloseMenu:(NSMenu *)menu withEvent:(NSEvent *)event;

- (NSMenu *)actionMenu;
- (void)copy:(id)sender;

@end

struct ActionMenuResult {
    WKRetainPtr<WKHitTestResultRef> hitTestResult;
    _WKActionMenuType type;
    RetainPtr<NSArray> defaultMenuItems;
    WKRetainPtr<WKTypeRef> userData;
};

@interface ActionMenusTestWKView : WKView {
    ActionMenuResult _actionMenuResult;
    RetainPtr<NSArray> _overrideItems;
    BOOL _shouldHaveUserData;
}

@property (nonatomic, assign, setter=_setActionMenuResult:) ActionMenuResult _actionMenuResult;

@end

@implementation ActionMenusTestWKView

@synthesize _actionMenuResult=_actionMenuResult;

- (NSArray *)_actionMenuItemsForHitTestResult:(WKHitTestResultRef)hitTestResult withType:(_WKActionMenuType)type defaultActionMenuItems:(NSArray *)defaultMenuItems userData:(WKTypeRef)userData
{
    if (type != kWKActionMenuNone)
        EXPECT_GT(defaultMenuItems.count, (NSUInteger)0);

    // Clients should be able to pass userData from the Web to UI process, between
    // the WKBundlePageContextMenuClient's prepareForActionMenu, and here.
    // http://trac.webkit.org/changeset/175444
    if (_shouldHaveUserData) {
        EXPECT_NOT_NULL(userData);
        EXPECT_EQ(WKDictionaryGetTypeID(), WKGetTypeID(userData));
        WKRetainPtr<WKStringRef> hasLinkKey = adoptWK(WKStringCreateWithUTF8CString("hasLinkURL"));
        WKTypeRef hasLinkValue = WKDictionaryGetItemForKey((WKDictionaryRef)userData, hasLinkKey.get());
        EXPECT_NOT_NULL(hasLinkValue);
        EXPECT_EQ(WKBooleanGetTypeID(), WKGetTypeID(hasLinkValue));
        WKRetainPtr<WKURLRef> absoluteLinkURL = adoptWK(WKHitTestResultCopyAbsoluteLinkURL(hitTestResult));
        EXPECT_EQ(!!absoluteLinkURL, WKBooleanGetValue((WKBooleanRef)hasLinkValue));
    } else
        EXPECT_NULL(userData);

    _actionMenuResult.hitTestResult = hitTestResult;
    _actionMenuResult.type = type;
    _actionMenuResult.defaultMenuItems = defaultMenuItems;
    _actionMenuResult.userData = userData;
    return _overrideItems ? _overrideItems.get() : defaultMenuItems;
}

- (void)runMenuSequenceAtPoint:(NSPoint)point preDidCloseMenuHandler:(void(^)(void))preDidCloseMenuHandler
{
    __block bool didFinishSequence = false;

    NSMenu *actionMenu = self.actionMenu;
    RetainPtr<NSEvent> event = [NSEvent mouseEventWithType:NSLeftMouseDown location:point modifierFlags:0 timestamp:0 windowNumber:self.window.windowNumber context:0 eventNumber:0 clickCount:0 pressure:0];

    dispatch_async(dispatch_get_main_queue(), ^{
        [self prepareForMenu:actionMenu withEvent:event.get()];
    });

    dispatch_async(dispatch_get_main_queue(), ^{
        _shouldHaveUserData = YES;
        [[actionMenu delegate] menuNeedsUpdate:actionMenu];
        _shouldHaveUserData = NO;
    });

    dispatch_async(dispatch_get_main_queue(), ^{
        [self willOpenMenu:actionMenu withEvent:event.get()];
    });

    void (^copiedPreDidCloseMenuHandler)() = Block_copy(preDidCloseMenuHandler);
    dispatch_async(dispatch_get_main_queue(), ^{
        copiedPreDidCloseMenuHandler();
        Block_release(copiedPreDidCloseMenuHandler);
        [self didCloseMenu:actionMenu withEvent:event.get()];
        [self mouseDown:event.get()];
        didFinishSequence = true;
    });

    TestWebKitAPI::Util::run(&didFinishSequence);
}

- (void)_setOverrideActionMenuItems:(NSArray *)overrideItems
{
    _overrideItems = overrideItems;
}

@end

namespace TestWebKitAPI {

struct ActiveDownloadContext {
    WKRetainPtr<WKStringRef> path;
    bool shouldCheckForImage = 0;
};

static void didFinishLoadForFrameCallback(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    didFinishLoad = true;
}

static void didFinishDownloadCallback(WKContextRef context, WKDownloadRef download, const void *clientInfo)
{
    WKStringRef wkActiveDownloadPath = ((ActiveDownloadContext*)clientInfo)->path.get();
    size_t length = WKStringGetLength(wkActiveDownloadPath) + 1;
    char *activeDownloadPath = (char *)calloc(1, length);
    WKStringGetUTF8CString(wkActiveDownloadPath, activeDownloadPath, length);

    if (((ActiveDownloadContext*)clientInfo)->shouldCheckForImage) {
        RetainPtr<NSImage> image = adoptNS([[NSImage alloc] initWithContentsOfFile:[NSString stringWithUTF8String:activeDownloadPath]]);

        EXPECT_EQ(215, [image size].width);
        EXPECT_EQ(174, [image size].height);
    }

    didFinishDownload = true;
}

static void didCreateDownloadDestinationCallback(WKContextRef context, WKDownloadRef download, WKStringRef path, const void *clientInfo)
{
    ((ActiveDownloadContext*)clientInfo)->path = path;
}

static NSString *watchPasteboardForString()
{
    [[NSPasteboard generalPasteboard] clearContents];

    while (true) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
        NSString *pasteboardString = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
        if (pasteboardString)
            return pasteboardString;
    }
}

static NSImage *watchPasteboardForImage()
{
    [[NSPasteboard generalPasteboard] clearContents];

    while (true) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
        NSArray *pasteboardItems = [[NSPasteboard generalPasteboard] readObjectsForClasses:@[ [NSImage class] ] options:nil];
        if (pasteboardItems && pasteboardItems.count)
            return pasteboardItems.lastObject;
    }
}

struct JavaScriptStringCallbackContext {
    JavaScriptStringCallbackContext()
        : didFinish(false)
    {
    }

    bool didFinish;
    JSRetainPtr<JSStringRef> actualString;
};

struct JavaScriptBoolCallbackContext {
    JavaScriptBoolCallbackContext()
        : didFinish(false)
    {
    }

    bool didFinish;
    bool value;
};

static void javaScriptStringCallback(WKSerializedScriptValueRef resultSerializedScriptValue, WKErrorRef error, void* ctx)
{
    EXPECT_NOT_NULL(resultSerializedScriptValue);

    JavaScriptStringCallbackContext* context = static_cast<JavaScriptStringCallbackContext*>(ctx);

    JSGlobalContextRef scriptContext = JSGlobalContextCreate(0);
    EXPECT_NOT_NULL(scriptContext);

    JSValueRef scriptValue = WKSerializedScriptValueDeserialize(resultSerializedScriptValue, scriptContext, 0);
    EXPECT_NOT_NULL(scriptValue);

    context->actualString.adopt(JSValueToStringCopy(scriptContext, scriptValue, 0));
    EXPECT_NOT_NULL(context->actualString.get());

    context->didFinish = true;
    
    JSGlobalContextRelease(scriptContext);
    
    EXPECT_NULL(error);
}

static void javaScriptBoolCallback(WKSerializedScriptValueRef resultSerializedScriptValue, WKErrorRef error, void* ctx)
{
    EXPECT_NOT_NULL(resultSerializedScriptValue);

    JavaScriptBoolCallbackContext* context = static_cast<JavaScriptBoolCallbackContext*>(ctx);

    JSGlobalContextRef scriptContext = JSGlobalContextCreate(0);
    EXPECT_NOT_NULL(scriptContext);

    JSValueRef scriptValue = WKSerializedScriptValueDeserialize(resultSerializedScriptValue, scriptContext, 0);
    EXPECT_NOT_NULL(scriptValue);

    EXPECT_TRUE(JSValueIsBoolean(scriptContext, scriptValue));

    context->value = JSValueToBoolean(scriptContext, scriptValue);
    context->didFinish = true;
    
    JSGlobalContextRelease(scriptContext);
    
    EXPECT_NULL(error);
}

static std::unique_ptr<char[]> callJavaScriptReturningString(WKPageRef page, const char* js)
{
    JavaScriptStringCallbackContext context;
    WKPageRunJavaScriptInMainFrame(page, Util::toWK(js).get(), &context, javaScriptStringCallback);
    Util::run(&context.didFinish);

    size_t bufferSize = JSStringGetMaximumUTF8CStringSize(context.actualString.get());
    auto buffer = std::make_unique<char[]>(bufferSize);
    JSStringGetUTF8CString(context.actualString.get(), buffer.get(), bufferSize);
    return buffer;
}

static bool callJavaScriptReturningBool(WKPageRef page, const char* js)
{
    JavaScriptBoolCallbackContext context;
    WKPageRunJavaScriptInMainFrame(page, Util::toWK(js).get(), &context, javaScriptBoolCallback);
    Util::run(&context.didFinish);
    return context.value;
}

static void watchEditableAreaForString(WKPageRef page, const char *areaName, const char *watchString)
{
    while (true) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
        auto buffer = callJavaScriptReturningString(page, [[NSString stringWithFormat:@"editableAreaString('%s')", areaName] UTF8String]);

        if (!strcmp(buffer.get(), watchString))
            return;
    }
}

static void waitForVideoReady(WKPageRef page)
{
    while (true) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
        if (callJavaScriptReturningBool(page, "isVideoReady()"))
            return;
    }
}

static NSString *retrieveSelection(WKPageRef page)
{
    auto buffer = callJavaScriptReturningString(page, "stringifySelection()");
    return [NSString stringWithUTF8String:buffer.get()];
}

static NSString *retrieveSelectionInElement(WKPageRef page, const char *areaName)
{
    auto buffer = callJavaScriptReturningString(page, [[NSString stringWithFormat:@"stringifySelectionInElement('%s')", areaName] UTF8String]);
    return [NSString stringWithUTF8String:buffer.get()];
}

static void performMenuItemAtIndexOfTypeAsync(NSMenu *menu, NSInteger index, int type)
{
    EXPECT_LT(index, menu.numberOfItems);
    if (index >= menu.numberOfItems)
        return;
    NSMenuItem *menuItem = [menu itemAtIndex:index];
    EXPECT_NOT_NULL(menuItem);
    EXPECT_EQ(type, [menuItem tag]);
    EXPECT_TRUE([menuItem isEnabled]);
    [menuItem.target performSelector:menuItem.action withObject:menuItem afterDelay:0];
}

static void ensureMenuItemAtIndexOfTypeIsDisabled(NSMenu *menu, NSInteger index, int type)
{
    EXPECT_LT(index, menu.numberOfItems);
    if (index >= menu.numberOfItems)
        return;
    NSMenuItem *menuItem = [menu itemAtIndex:index];
    EXPECT_NOT_NULL(menuItem);
    EXPECT_EQ(type, [menuItem tag]);
    EXPECT_FALSE([menuItem isEnabled]);
}

enum class TargetType {
    Word,
    Phrase,
    Address,
    Date,
    PhoneNumber,
    ContentEditableWords,
    ContentEditablePhrase,
    TextInputWords,
    TextInputPhrase,
    TextAreaWords,
    TextAreaPhrase,
    Image,
    ImageAsLink,
    HTTPLink,
    FTPLink,
    MailtoLink,
    JavaScriptLink,
    PageOverlay,
    PageWhitespace,
    Video,
    MSEVideo,
};

static NSPoint windowPointForTarget(TargetType target)
{
    NSPoint contentPoint;
    switch (target) {
    case TargetType::Word:
        contentPoint = NSMakePoint(0, 0);
        break;
    case TargetType::Phrase:
        contentPoint = NSMakePoint(200, 0);
        break;
    case TargetType::Address:
        contentPoint = NSMakePoint(0, 50);
        break;
    case TargetType::Date:
        contentPoint = NSMakePoint(200, 50);
        break;
    case TargetType::PhoneNumber:
        contentPoint = NSMakePoint(400, 50);
        break;
    case TargetType::ContentEditableWords:
        contentPoint = NSMakePoint(0, 150);
        break;
    case TargetType::ContentEditablePhrase:
        contentPoint = NSMakePoint(0, 200);
        break;
    case TargetType::TextInputWords:
        contentPoint = NSMakePoint(200, 150);
        break;
    case TargetType::TextInputPhrase:
        contentPoint = NSMakePoint(200, 200);
        break;
    case TargetType::TextAreaWords:
        contentPoint = NSMakePoint(400, 150);
        break;
    case TargetType::TextAreaPhrase:
        contentPoint = NSMakePoint(400, 200);
        break;
    case TargetType::Image:
        contentPoint = NSMakePoint(0, 250);
        break;
    case TargetType::ImageAsLink:
        contentPoint = NSMakePoint(200, 250);
        break;
    case TargetType::HTTPLink:
        contentPoint = NSMakePoint(0, 300);
        break;
    case TargetType::FTPLink:
        contentPoint = NSMakePoint(100, 300);
        break;
    case TargetType::MailtoLink:
        contentPoint = NSMakePoint(200, 300);
        break;
    case TargetType::JavaScriptLink:
        contentPoint = NSMakePoint(300, 300);
        break;
    case TargetType::PageOverlay:
        contentPoint = NSMakePoint(750, 100);
        break;
    case TargetType::PageWhitespace:
        contentPoint = NSMakePoint(650, 0);
        break;
    case TargetType::Video:
        contentPoint = NSMakePoint(0, 350);
        break;
    case TargetType::MSEVideo:
        contentPoint = NSMakePoint(200, 350);
        break;
    }

    return NSMakePoint(contentPoint.x + 8, 600 - contentPoint.y - 8);
}

// FIXME: Ideally, each of these would be able to run as its own subtest in a suite, sharing a WKView (for performance reasons),
// but we cannot because run-api-tests explicitly runs each test in a separate process. So, we use a single test for many tests instead.
TEST(WebKit2, ActionMenusTest)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextForInjectedBundleTest("ActionMenusTest"));

    WKRetainPtr<WKPageGroupRef> pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(Util::toWK("ActionMenusTestGroup").get()));
    WKPreferencesRef preferences = WKPageGroupGetPreferences(pageGroup.get());
    WKPreferencesSetMediaSourceEnabled(preferences, true);
    WKPreferencesSetFileAccessFromFileURLsAllowed(preferences, true);

    PlatformWebView platformWebView(context.get(), pageGroup.get(), [ActionMenusTestWKView class]);
    RetainPtr<ActionMenusTestWKView> wkView = (ActionMenusTestWKView *)platformWebView.platformView();

    if (![wkView respondsToSelector:@selector(setActionMenu:)])
        return;

    WKPageLoaderClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    loaderClient.base.version = 0;
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrameCallback;
    WKPageSetPageLoaderClient([wkView pageRef], &loaderClient.base);

    ActiveDownloadContext activeDownloadContext;

    WKContextDownloadClientV0 downloadClient;
    memset(&downloadClient, 0, sizeof(downloadClient));
    downloadClient.base.version = 0;
    downloadClient.base.clientInfo = &activeDownloadContext;
    downloadClient.didFinish = didFinishDownloadCallback;
    downloadClient.didCreateDestination = didCreateDownloadDestinationCallback;
    WKContextSetDownloadClient(context.get(), &downloadClient.base);

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("action-menu-targets", "html"));
    WKPageLoadURL([wkView pageRef], url.get());

    Util::run(&didFinishLoad);

    // Read-only text.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::Word) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuReadOnlyText, [wkView _actionMenuResult].type);
        EXPECT_WK_STREQ(@"word", retrieveSelection([wkView pageRef]));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 0, kWKContextActionItemTagCopyText);
        EXPECT_WK_STREQ(@"word", watchPasteboardForString());
    }];

    // Read-only text, on a phrase.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::Phrase) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuReadOnlyText, [wkView _actionMenuResult].type);
        EXPECT_WK_STREQ(@"New York", retrieveSelection([wkView pageRef]));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 0, kWKContextActionItemTagCopyText);
        EXPECT_WK_STREQ(@"New York", watchPasteboardForString());
    }];

    // Read-only text, on an address.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::Address) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuDataDetectedItem, [wkView _actionMenuResult].type);

        // Addresses don't get selected, because they immediately show a TextIndicator.
        EXPECT_WK_STREQ(@"<no selection>", retrieveSelection([wkView pageRef]));
    }];

    // Read-only text, on a date.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::Date) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuDataDetectedItem, [wkView _actionMenuResult].type);

        // Dates don't get selected, because they immediately show a TextIndicator.
        EXPECT_WK_STREQ(@"<no selection>", retrieveSelection([wkView pageRef]));
    }];

    // Read-only text, on a phone number.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::PhoneNumber) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuDataDetectedItem, [wkView _actionMenuResult].type);
        EXPECT_WK_STREQ(@"(408) 996-1010", retrieveSelection([wkView pageRef]));
    }];

    // Copy from a contentEditable div.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::ContentEditableWords) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuEditableText, [wkView _actionMenuResult].type);
        EXPECT_WK_STREQ(@"editable", retrieveSelection([wkView pageRef]));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 0, kWKContextActionItemTagCopyText);
        EXPECT_WK_STREQ(@"editable", watchPasteboardForString());
    }];

    // Copy a phrase from a contentEditable div.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::ContentEditablePhrase) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuEditableText, [wkView _actionMenuResult].type);
        EXPECT_WK_STREQ(@"New York", retrieveSelection([wkView pageRef]));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 0, kWKContextActionItemTagCopyText);
        EXPECT_WK_STREQ(@"New York", watchPasteboardForString());
    }];

    // Paste on top of the text in the contentEditable div.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::ContentEditableWords) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuEditableText, [wkView _actionMenuResult].type);
        [[NSPasteboard generalPasteboard] clearContents];
        [[NSPasteboard generalPasteboard] setString:@"pasted string" forType:NSPasteboardTypeString];
        EXPECT_WK_STREQ(@"editable", retrieveSelection([wkView pageRef]));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 2, kWKContextActionItemTagPaste);

        // Now check and see if our paste succeeded. It should only replace one 'editable'.
        watchEditableAreaForString([wkView pageRef], "editable1", "pasted string editable editable editable");
    }];

    // Paste on top of a phrase in the contentEditable div.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::ContentEditablePhrase) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuEditableText, [wkView _actionMenuResult].type);
        [[NSPasteboard generalPasteboard] clearContents];
        [[NSPasteboard generalPasteboard] setString:@"pasted over phrase" forType:NSPasteboardTypeString];
        EXPECT_WK_STREQ(@"New York", retrieveSelection([wkView pageRef]));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 2, kWKContextActionItemTagPaste);

        // Now check and see if our paste succeeded, and replaced the whole phrase.
        watchEditableAreaForString([wkView pageRef], "editable2", "pasted over phrase some words");
    }];

    // Copy from an <input>.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::TextInputWords) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuEditableText, [wkView _actionMenuResult].type);
        EXPECT_WK_STREQ(@"editable", retrieveSelectionInElement([wkView pageRef], "input1"));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 0, kWKContextActionItemTagCopyText);
        EXPECT_WK_STREQ(@"editable", watchPasteboardForString());
    }];

    // Copy a phrase from an <input>.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::TextInputPhrase) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuEditableText, [wkView _actionMenuResult].type);
        EXPECT_WK_STREQ(@"New York", retrieveSelectionInElement([wkView pageRef], "input2"));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 0, kWKContextActionItemTagCopyText);
        EXPECT_WK_STREQ(@"New York", watchPasteboardForString());
    }];

    // Paste on top of the editable text in an <input>.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::TextInputWords) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuEditableText, [wkView _actionMenuResult].type);
        [[NSPasteboard generalPasteboard] clearContents];
        [[NSPasteboard generalPasteboard] setString:@"pasted string" forType:NSPasteboardTypeString];
        EXPECT_WK_STREQ(@"editable", retrieveSelectionInElement([wkView pageRef], "input1"));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 2, kWKContextActionItemTagPaste);

        // Now check and see if our paste succeeded. It should only replace one 'editable'.
        watchEditableAreaForString([wkView pageRef], "input1", "pasted string editable editable editable");
    }];

    // Paste on top of the editable text, on a phrase in an <input>.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::TextInputPhrase) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuEditableText, [wkView _actionMenuResult].type);
        [[NSPasteboard generalPasteboard] clearContents];
        [[NSPasteboard generalPasteboard] setString:@"pasted over phrase" forType:NSPasteboardTypeString];
        EXPECT_WK_STREQ(@"New York", retrieveSelectionInElement([wkView pageRef], "input2"));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 2, kWKContextActionItemTagPaste);

        // Now check and see if our paste succeeded, and replaced the whole phrase.
        watchEditableAreaForString([wkView pageRef], "input2", "pasted over phrase some words");
    }];

    // Copy from a <textarea>.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::TextAreaWords) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuEditableText, [wkView _actionMenuResult].type);
        EXPECT_WK_STREQ(@"editable", retrieveSelectionInElement([wkView pageRef], "textarea1"));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 0, kWKContextActionItemTagCopyText);
        EXPECT_WK_STREQ(@"editable", watchPasteboardForString());
    }];

    // Copy a phrase from a <textarea>.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::TextAreaPhrase) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuEditableText, [wkView _actionMenuResult].type);
        EXPECT_WK_STREQ(@"New York", retrieveSelectionInElement([wkView pageRef], "textarea2"));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 0, kWKContextActionItemTagCopyText);
        EXPECT_WK_STREQ(@"New York", watchPasteboardForString());
    }];

    // Paste on top of the editable text in a <textarea>.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::TextAreaWords) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuEditableText, [wkView _actionMenuResult].type);
        [[NSPasteboard generalPasteboard] clearContents];
        [[NSPasteboard generalPasteboard] setString:@"pasted" forType:NSPasteboardTypeString];
        EXPECT_WK_STREQ(@"editable", retrieveSelectionInElement([wkView pageRef], "textarea1"));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 2, kWKContextActionItemTagPaste);

        // Now check and see if our paste succeeded. It should only replace one 'editable'.
        watchEditableAreaForString([wkView pageRef], "textarea1", "pasted editable editable editable");
    }];

    // Paste on top of the editable text, on a phrase in a <textarea>.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::TextAreaPhrase) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuEditableText, [wkView _actionMenuResult].type);
        [[NSPasteboard generalPasteboard] clearContents];
        [[NSPasteboard generalPasteboard] setString:@"pasted over phrase" forType:NSPasteboardTypeString];
        EXPECT_WK_STREQ(@"New York", retrieveSelectionInElement([wkView pageRef], "textarea2"));
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 2, kWKContextActionItemTagPaste);

        // Now check and see if our paste succeeded, and replaced the whole phrase.
        watchEditableAreaForString([wkView pageRef], "textarea2", "pasted over phrase some words");
    }];

    // Copy an image.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::Image) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuImage, [wkView _actionMenuResult].type);

        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 0, kWKContextActionItemTagCopyImage);
        NSImage *image = watchPasteboardForImage();

        EXPECT_EQ(215, image.size.width);
        EXPECT_EQ(174, image.size.height);
    }];

    // Download an image.
    activeDownloadContext.shouldCheckForImage = true;
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::Image) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuImage, [wkView _actionMenuResult].type);

        didFinishDownload = false;
        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 2, kWKContextActionItemTagSaveImageToDownloads);
        Util::run(&didFinishDownload);
    }];
    activeDownloadContext.shouldCheckForImage = false;

    // Images that are also links should be treated as links.
    // http://trac.webkit.org/changeset/175701
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::ImageAsLink) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuLink, [wkView _actionMenuResult].type);
    }];

    waitForVideoReady([wkView pageRef]);

    // Copy a video URL.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::Video) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuVideo, [wkView _actionMenuResult].type);

        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 0, kWKContextActionItemTagCopyVideoURL);
        NSString *videoURL = watchPasteboardForString();
        EXPECT_WK_STREQ(@"test.mp4", [videoURL lastPathComponent]);
    }];

    // Copying a video URL for a non-downloadable video should result in copying the page URL instead.
    // http://trac.webkit.org/changeset/176131
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::MSEVideo) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuVideo, [wkView _actionMenuResult].type);

        performMenuItemAtIndexOfTypeAsync([wkView actionMenu], 0, kWKContextActionItemTagCopyVideoURL);
        NSString *videoURL = watchPasteboardForString();
        EXPECT_WK_STREQ(@"action-menu-targets.html", [videoURL lastPathComponent]);

        // Also, the download menu item should be disabled for non-downloadable video.
        ensureMenuItemAtIndexOfTypeIsDisabled([wkView actionMenu], 2, kWKContextActionItemTagSaveVideoToDownloads);
    }];

    // HTTP link.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::HTTPLink) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuLink, [wkView _actionMenuResult].type);

        // Invoking an action menu should dismiss any existing selection.
        // http://trac.webkit.org/changeset/175753
        EXPECT_WK_STREQ(@"<no selection>", retrieveSelection([wkView pageRef]));
    }];

    // Mailto link.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::MailtoLink) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuMailtoLink, [wkView _actionMenuResult].type);

        // Data detected links don't get a selection nor special indication, for consistency with HTTP links.
        EXPECT_WK_STREQ(@"<no selection>", retrieveSelection([wkView pageRef]));
    }];

    // Non-HTTP(S), non-mailto links should fall back to the text menu.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::FTPLink) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuReadOnlyText, [wkView _actionMenuResult].type);
        EXPECT_WK_STREQ(@"ftp", retrieveSelection([wkView pageRef]));
    }];

    // JavaScript links should not be executed, and should fall back to the text menu.
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::JavaScriptLink) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuReadOnlyText, [wkView _actionMenuResult].type);
        EXPECT_WK_STREQ(@"javascript", retrieveSelection([wkView pageRef]));
        EXPECT_FALSE(callJavaScriptReturningBool([wkView pageRef], "wasFailCalled()"));
    }];

    // Clients should be able to customize the menu by overriding WKView's _actionMenuItemsForHitTestResult.
    // http://trac.webkit.org/changeset/174908
    RetainPtr<NSMenuItem> item = adoptNS([[NSMenuItem alloc] initWithTitle:@"Some Action" action:@selector(copy:) keyEquivalent:@""]);
    [wkView _setOverrideActionMenuItems:@[ item.get() ]];
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::Image) preDidCloseMenuHandler:^() {
        EXPECT_EQ(1, [wkView actionMenu].numberOfItems);
        EXPECT_WK_STREQ(@"Some Action", [[wkView actionMenu] itemAtIndex:0].title);
    }];
    [wkView _setOverrideActionMenuItems:nil];

    // Clients should be able to customize the DataDetectors actions by implementing
    // WKBundlePageOverlayClient's prepareForActionMenu callback.
    // http://trac.webkit.org/changeset/176086
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::PageOverlay) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuDataDetectedItem, [wkView _actionMenuResult].type);
    }];

    // No menu should be built for whitespace (except in editable areas).
    [wkView runMenuSequenceAtPoint:windowPointForTarget(TargetType::PageWhitespace) preDidCloseMenuHandler:^() {
        EXPECT_EQ(kWKActionMenuNone, [wkView _actionMenuResult].type);
        EXPECT_EQ(0, [wkView actionMenu].numberOfItems);
    }];
}

} // namespace TestWebKitAPI
