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

#import "SettingsController.h"

#import "AppDelegate.h"
#import "BrowserWindowController.h"
#import "MockContentFilterSettings.h"

static NSString * const defaultURL = @"http://www.webkit.org/";
static NSString * const DefaultURLPreferenceKey = @"DefaultURL";

static NSString * const UseWebKit2ByDefaultPreferenceKey = @"UseWebKit2ByDefault";
static NSString * const LayerBordersVisiblePreferenceKey = @"LayerBordersVisible";
static NSString * const SimpleLineLayoutDebugBordersEnabledPreferenceKey = @"SimpleLineLayoutDebugBordersEnabled";
static NSString * const TiledScrollingIndicatorVisiblePreferenceKey = @"TiledScrollingIndicatorVisible";
static NSString * const ResourceUsageOverlayVisiblePreferenceKey = @"ResourceUsageOverlayVisible";
static NSString * const IncrementalRenderingSuppressedPreferenceKey = @"IncrementalRenderingSuppressed";
static NSString * const AcceleratedDrawingEnabledPreferenceKey = @"AcceleratedDrawingEnabled";

static NSString * const ContentFilteringEnabledKey = @"ContentFilteringEnabled";
static NSString * const ContentFilterDecisionKey = @"ContentFilterDecision";
static NSString * const ContentFilterDecisionPointKey = @"ContentFilterDecisionPoint";

static NSString * const NonFastScrollableRegionOverlayVisiblePreferenceKey = @"NonFastScrollableRegionOverlayVisible";
static NSString * const WheelEventHandlerRegionOverlayVisiblePreferenceKey = @"WheelEventHandlerRegionOverlayVisible";

static NSString * const UseTransparentWindowsPreferenceKey = @"UseTransparentWindows";
static NSString * const UsePaginatedModePreferenceKey = @"UsePaginatedMode";
static NSString * const EnableSubPixelCSSOMMetricsPreferenceKey = @"EnableSubPixelCSSOMMetrics";

// This default name intentionally overlaps with the key that WebKit2 checks when creating a view.
static NSString * const UseRemoteLayerTreeDrawingAreaPreferenceKey = @"WebKit2UseRemoteLayerTreeDrawingArea";

static NSString * const PerWindowWebProcessesDisabledKey = @"PerWindowWebProcessesDisabled";

typedef NS_ENUM(NSInteger, DebugOverylayMenuItemTag) {
    NonFastScrollableRegionOverlayTag = 100,
    WheelEventHandlerRegionOverlayTag
};

@implementation SettingsController

@synthesize menu=_menu;

+ (instancetype)shared
{
    static SettingsController *sharedSettingsController;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedSettingsController = [[super alloc] init];
    });

    return sharedSettingsController;
}

- (NSMenu *)menu
{
    if (!_menu)
        [self _populateMenu];

    return _menu;
}

- (void)_addItemWithTitle:(NSString *)title action:(SEL)action indented:(BOOL)indented
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:@""];
    [item setTarget:self];
    if (indented)
        [item setIndentationLevel:1];
    [_menu addItem:item];
    [item release];
}

- (void)_addHeaderWithTitle:(NSString *)title
{
    [_menu addItem:[NSMenuItem separatorItem]];
    [_menu addItem:[[[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""] autorelease]];
}

- (void)_populateMenu
{
    _menu = [[NSMenu alloc] initWithTitle:@"Settings"];

    [self _addItemWithTitle:@"Use WebKit2 By Default" action:@selector(toggleUseWebKit2ByDefault:) indented:NO];
    [self _addItemWithTitle:@"Set Default URL to Current URL" action:@selector(setDefaultURLToCurrentURL:) indented:NO];

    [_menu addItem:[NSMenuItem separatorItem]];

    [self _addItemWithTitle:@"Use Transparent Windows" action:@selector(toggleUseTransparentWindows:) indented:NO];
    [self _addItemWithTitle:@"Use Paginated Mode" action:@selector(toggleUsePaginatedMode:) indented:NO];
    [self _addItemWithTitle:@"Show Layer Borders" action:@selector(toggleShowLayerBorders:) indented:NO];
    [self _addItemWithTitle:@"Show Simple Line Layout Borders" action:@selector(toggleSimpleLineLayoutDebugBordersEnabled:) indented:NO];
    [self _addItemWithTitle:@"Suppress Incremental Rendering in New Windows" action:@selector(toggleIncrementalRenderingSuppressed:) indented:NO];
    [self _addItemWithTitle:@"Enable Accelerated Drawing" action:@selector(toggleAcceleratedDrawingEnabled:) indented:NO];
    
    [_menu addItem:[self _contentFilteringMenuItem]];

    [self _addHeaderWithTitle:@"WebKit2-only Settings"];

    [self _addItemWithTitle:@"Show Tiled Scrolling Indicator" action:@selector(toggleShowTiledScrollingIndicator:) indented:YES];
    [self _addItemWithTitle:@"Use UI-Side Compositing" action:@selector(toggleUseUISideCompositing:) indented:YES];
    [self _addItemWithTitle:@"Disable Per-Window Web Processes" action:@selector(togglePerWindowWebProcessesDisabled:) indented:YES];
    [self _addItemWithTitle:@"Show Resource Usage Overlay" action:@selector(toggleShowResourceUsageOverlay:) indented:YES];

    NSMenuItem *debugOverlaysSubmenuItem = [[NSMenuItem alloc] initWithTitle:@"Debug Overlays" action:nil keyEquivalent:@""];
    NSMenu *debugOverlaysMenu = [[NSMenu alloc] initWithTitle:@"Debug Overlays"];
    [debugOverlaysSubmenuItem setSubmenu:debugOverlaysMenu];

    NSMenuItem *nonFastScrollableRegionItem = [[NSMenuItem alloc] initWithTitle:@"Non-fast Scrollable Region" action:@selector(toggleDebugOverlay:) keyEquivalent:@""];
    [nonFastScrollableRegionItem setTag:NonFastScrollableRegionOverlayTag];
    [nonFastScrollableRegionItem setTarget:self];
    [debugOverlaysMenu addItem:[nonFastScrollableRegionItem autorelease]];

    NSMenuItem *wheelEventHandlerRegionItem = [[NSMenuItem alloc] initWithTitle:@"Wheel Event Handler Region" action:@selector(toggleDebugOverlay:) keyEquivalent:@""];
    [wheelEventHandlerRegionItem setTag:WheelEventHandlerRegionOverlayTag];
    [wheelEventHandlerRegionItem setTarget:self];
    [debugOverlaysMenu addItem:[wheelEventHandlerRegionItem autorelease]];
    [debugOverlaysMenu release];
    
    [_menu addItem:debugOverlaysSubmenuItem];
    [debugOverlaysSubmenuItem release];

    [self _addHeaderWithTitle:@"WebKit1-only Settings"];
    [self _addItemWithTitle:@"Enable Subpixel CSSOM Metrics" action:@selector(toggleEnableSubPixelCSSOMMetrics:) indented:YES];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL action = [menuItem action];

    if (action == @selector(toggleUseWebKit2ByDefault:))
        [menuItem setState:[self useWebKit2ByDefault] ? NSOnState : NSOffState];
    else if (action == @selector(toggleUseTransparentWindows:))
        [menuItem setState:[self useTransparentWindows] ? NSOnState : NSOffState];
    else if (action == @selector(toggleUsePaginatedMode:))
        [menuItem setState:[self usePaginatedMode] ? NSOnState : NSOffState];
    else if (action == @selector(toggleShowLayerBorders:))
        [menuItem setState:[self layerBordersVisible] ? NSOnState : NSOffState];
    else if (action == @selector(toggleSimpleLineLayoutDebugBordersEnabled:))
        [menuItem setState:[self simpleLineLayoutDebugBordersEnabled] ? NSOnState : NSOffState];
    else if (action == @selector(toggleIncrementalRenderingSuppressed:))
        [menuItem setState:[self incrementalRenderingSuppressed] ? NSOnState : NSOffState];
    else if (action == @selector(toggleAcceleratedDrawingEnabled:))
        [menuItem setState:[self acceleratedDrawingEnabled] ? NSOnState : NSOffState];
    else if (action == @selector(toggleShowTiledScrollingIndicator:))
        [menuItem setState:[self tiledScrollingIndicatorVisible] ? NSOnState : NSOffState];
    else if (action == @selector(toggleShowResourceUsageOverlay:))
        [menuItem setState:[self resourceUsageOverlayVisible] ? NSOnState : NSOffState];
    else if (action == @selector(toggleUseUISideCompositing:))
        [menuItem setState:[self useUISideCompositing] ? NSOnState : NSOffState];
    else if (action == @selector(togglePerWindowWebProcessesDisabled:))
        [menuItem setState:[self perWindowWebProcessesDisabled] ? NSOnState : NSOffState];
    else if (action == @selector(toggleEnableSubPixelCSSOMMetrics:))
        [menuItem setState:[self subPixelCSSOMMetricsEnabled] ? NSOnState : NSOffState];
    else if (action == @selector(toggleDebugOverlay:))
        [menuItem setState:[self debugOverlayVisible:menuItem] ? NSOnState : NSOffState];
    else if (action == @selector(toggleContentFilteringEnabled:))
        [menuItem setState:[self contentFilteringEnabled] ? NSOnState : NSOffState];
    else if (action == @selector(setContentFilteringDecision:)) {
        [menuItem setState:[self contentFilteringDecision] == menuItem.tag ? NSOnState : NSOffState];
        if (![self contentFilteringEnabled])
            return NO;
    } else if (action == @selector(setContentFilteringDecisionPoint:)) {
        [menuItem setState:[self contentFilteringDecisionPoint] == menuItem.tag ? NSOnState : NSOffState];
        if (![self contentFilteringEnabled])
            return NO;
    }

    return YES;
}

- (void)_toggleBooleanDefault:(NSString *)defaultName
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setBool:![defaults boolForKey:defaultName] forKey:defaultName];

    [(BrowserAppDelegate *)[[NSApplication sharedApplication] delegate] didChangeSettings];
}

- (void)toggleUseWebKit2ByDefault:(id)sender
{
    [self _toggleBooleanDefault:UseWebKit2ByDefaultPreferenceKey];
}

- (BOOL)useWebKit2ByDefault
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:UseWebKit2ByDefaultPreferenceKey];
}

- (void)toggleUseTransparentWindows:(id)sender
{
    [self _toggleBooleanDefault:UseTransparentWindowsPreferenceKey];
}

- (BOOL)useTransparentWindows
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:UseTransparentWindowsPreferenceKey];
}

- (void)toggleUsePaginatedMode:(id)sender
{
    [self _toggleBooleanDefault:UsePaginatedModePreferenceKey];
}

- (BOOL)usePaginatedMode
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:UsePaginatedModePreferenceKey];
}

- (void)toggleUseUISideCompositing:(id)sender
{
    [self _toggleBooleanDefault:UseRemoteLayerTreeDrawingAreaPreferenceKey];
}

- (BOOL)useUISideCompositing
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:UseRemoteLayerTreeDrawingAreaPreferenceKey];
}

- (void)togglePerWindowWebProcessesDisabled:(id)sender
{
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:self.perWindowWebProcessesDisabled ? @"Are you sure you want to switch to per-window web processes?" : @"Are you sure you want to switch to a single web process?"];
    [alert setInformativeText:@"This requires quitting and relaunching MiniBrowser. I'll do the quitting. You will have to do the relaunching."];
    [alert addButtonWithTitle:@"Switch and Quit"];
    [alert addButtonWithTitle:@"Cancel"];

    if ([alert runModal] != NSAlertFirstButtonReturn)
        return;

    [self _toggleBooleanDefault:PerWindowWebProcessesDisabledKey];
    [NSApp terminate:self];
}

- (BOOL)perWindowWebProcessesDisabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:PerWindowWebProcessesDisabledKey];
}

- (void)toggleIncrementalRenderingSuppressed:(id)sender
{
    [self _toggleBooleanDefault:IncrementalRenderingSuppressedPreferenceKey];
}

- (BOOL)incrementalRenderingSuppressed
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:IncrementalRenderingSuppressedPreferenceKey];
}

- (void)toggleShowLayerBorders:(id)sender
{
    [self _toggleBooleanDefault:LayerBordersVisiblePreferenceKey];
}

- (BOOL)layerBordersVisible
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:LayerBordersVisiblePreferenceKey];
}

- (void)toggleSimpleLineLayoutDebugBordersEnabled:(id)sender
{
    [self _toggleBooleanDefault:SimpleLineLayoutDebugBordersEnabledPreferenceKey];
}

- (BOOL)simpleLineLayoutDebugBordersEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:SimpleLineLayoutDebugBordersEnabledPreferenceKey];
}

- (void)toggleAcceleratedDrawingEnabled:(id)sender
{
    [self _toggleBooleanDefault:AcceleratedDrawingEnabledPreferenceKey];
}

- (BOOL)acceleratedDrawingEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:AcceleratedDrawingEnabledPreferenceKey];
}

- (void)toggleShowTiledScrollingIndicator:(id)sender
{
    [self _toggleBooleanDefault:TiledScrollingIndicatorVisiblePreferenceKey];
}

- (void)toggleShowResourceUsageOverlay:(id)sender
{
    [self _toggleBooleanDefault:ResourceUsageOverlayVisiblePreferenceKey];
}

- (BOOL)tiledScrollingIndicatorVisible
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:TiledScrollingIndicatorVisiblePreferenceKey];
}

- (BOOL)resourceUsageOverlayVisible
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:ResourceUsageOverlayVisiblePreferenceKey];
}

- (void)toggleEnableSubPixelCSSOMMetrics:(id)sender
{
    [self _toggleBooleanDefault:EnableSubPixelCSSOMMetricsPreferenceKey];
}

- (BOOL)subPixelCSSOMMetricsEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:EnableSubPixelCSSOMMetricsPreferenceKey];
}

- (BOOL)nonFastScrollableRegionOverlayVisible
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:NonFastScrollableRegionOverlayVisiblePreferenceKey];
}

- (BOOL)wheelEventHandlerRegionOverlayVisible
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WheelEventHandlerRegionOverlayVisiblePreferenceKey];
}

- (NSString *)preferenceKeyForRegionOverlayTag:(NSUInteger)tag
{
    switch (tag) {
    case NonFastScrollableRegionOverlayTag:
        return NonFastScrollableRegionOverlayVisiblePreferenceKey;

    case WheelEventHandlerRegionOverlayTag:
        return WheelEventHandlerRegionOverlayVisiblePreferenceKey;
    }
    return nil;
}

- (void)toggleDebugOverlay:(id)sender
{
    NSString *preferenceKey = [self preferenceKeyForRegionOverlayTag:[sender tag]];
    if (preferenceKey)
        [self _toggleBooleanDefault:preferenceKey];
}

- (BOOL)debugOverlayVisible:(NSMenuItem *)menuItem
{
    NSString *preferenceKey = [self preferenceKeyForRegionOverlayTag:[menuItem tag]];
    if (preferenceKey)
        return [[NSUserDefaults standardUserDefaults] boolForKey:preferenceKey];

    return NO;
}

- (NSString *)defaultURL
{
    NSString *customDefaultURL = [[NSUserDefaults standardUserDefaults] stringForKey:DefaultURLPreferenceKey];
    if (customDefaultURL)
        return customDefaultURL;
    return defaultURL;
}

- (void)setDefaultURLToCurrentURL:(id)sender
{
    NSWindowController *windowController = [[NSApp keyWindow] windowController];
    NSString *customDefaultURL = nil;

    if ([windowController isKindOfClass:[BrowserWindowController class]])
        customDefaultURL = [[(BrowserWindowController *)windowController currentURL] absoluteString];

    if (customDefaultURL)
        [[NSUserDefaults standardUserDefaults] setObject:customDefaultURL forKey:DefaultURLPreferenceKey];
}

#pragma mark Content Filtering

- (NSMenuItem *)_contentFilteringMenuItem
{
    NSMenu *menu = [[[NSMenu alloc] initWithTitle:@"Content Filtering"] autorelease];

    NSMenuItem *enabledItem = [[[NSMenuItem alloc] initWithTitle:@"Enabled" action:@selector(toggleContentFilteringEnabled:) keyEquivalent:@""] autorelease];
    enabledItem.target = self;
    [menu addItem:enabledItem];

    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[[[NSMenuItem alloc] initWithTitle:@"Decision" action:nil keyEquivalent:@""] autorelease]];

    NSMenuItem *allowItem = [[[NSMenuItem alloc] initWithTitle:@"Allow" action:@selector(setContentFilteringDecision:) keyEquivalent:@""] autorelease];
    allowItem.target = self;
    allowItem.indentationLevel = 1;
    allowItem.tag = WebMockContentFilterDecisionAllow;
    [menu addItem:allowItem];

    NSMenuItem *blockItem = [[[NSMenuItem alloc] initWithTitle:@"Block" action:@selector(setContentFilteringDecision:) keyEquivalent:@""] autorelease];
    blockItem.target = self;
    blockItem.indentationLevel = 1;
    blockItem.tag = WebMockContentFilterDecisionBlock;
    [menu addItem:blockItem];

    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[[[NSMenuItem alloc] initWithTitle:@"Decision Point" action:nil keyEquivalent:@""] autorelease]];

    NSMenuItem *afterWillSendRequestItem = [[[NSMenuItem alloc] initWithTitle:@"After Will Send Request" action:@selector(setContentFilteringDecisionPoint:) keyEquivalent:@""] autorelease];
    afterWillSendRequestItem.target = self;
    afterWillSendRequestItem.indentationLevel = 1;
    afterWillSendRequestItem.tag = WebMockContentFilterDecisionPointAfterWillSendRequest;
    [menu addItem:afterWillSendRequestItem];
    
    NSMenuItem *afterRedirectItem = [[[NSMenuItem alloc] initWithTitle:@"After Redirect" action:@selector(setContentFilteringDecisionPoint:) keyEquivalent:@""] autorelease];
    afterRedirectItem.target = self;
    afterRedirectItem.indentationLevel = 1;
    afterRedirectItem.tag = WebMockContentFilterDecisionPointAfterRedirect;
    [menu addItem:afterRedirectItem];
    
    NSMenuItem *afterResponseItem = [[[NSMenuItem alloc] initWithTitle:@"After Response" action:@selector(setContentFilteringDecisionPoint:) keyEquivalent:@""] autorelease];
    afterResponseItem.target = self;
    afterResponseItem.indentationLevel = 1;
    afterResponseItem.tag = WebMockContentFilterDecisionPointAfterResponse;
    [menu addItem:afterResponseItem];
    
    NSMenuItem *afterAddDataItem = [[[NSMenuItem alloc] initWithTitle:@"After Add Data" action:@selector(setContentFilteringDecisionPoint:) keyEquivalent:@""] autorelease];
    afterAddDataItem.target = self;
    afterAddDataItem.indentationLevel = 1;
    afterAddDataItem.tag = WebMockContentFilterDecisionPointAfterAddData;
    [menu addItem:afterAddDataItem];
    
    NSMenuItem *afterFinishedAddingDataItem = [[[NSMenuItem alloc] initWithTitle:@"After Finished Adding Data" action:@selector(setContentFilteringDecisionPoint:) keyEquivalent:@""] autorelease];
    afterFinishedAddingDataItem.target = self;
    afterFinishedAddingDataItem.indentationLevel = 1;
    afterFinishedAddingDataItem.tag = WebMockContentFilterDecisionPointAfterFinishedAddingData;
    [menu addItem:afterFinishedAddingDataItem];
    
    NSMenuItem *neverMenuItem = [[[NSMenuItem alloc] initWithTitle:@"Never" action:@selector(setContentFilteringDecisionPoint:) keyEquivalent:@""] autorelease];
    neverMenuItem.target = self;
    neverMenuItem.indentationLevel = 1;
    neverMenuItem.tag = WebMockContentFilterDecisionPointNever;
    [menu addItem:neverMenuItem];
    
    NSMenuItem *submenuItem = [[NSMenuItem alloc] initWithTitle:@"Content Filtering" action:nil keyEquivalent:@""];
    [submenuItem setSubmenu:menu];
    return [submenuItem autorelease];
}

- (void)toggleContentFilteringEnabled:(id)sender
{
    [self _toggleBooleanDefault:ContentFilteringEnabledKey];
}

- (BOOL)contentFilteringEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:ContentFilteringEnabledKey];
}

- (void)setContentFilteringDecision:(id)sender
{
    [[NSUserDefaults standardUserDefaults] setInteger:[sender tag] forKey:ContentFilterDecisionKey];
    [(BrowserAppDelegate *)[[NSApplication sharedApplication] delegate] didChangeSettings];
}

- (WebMockContentFilterDecision)contentFilteringDecision
{
    NSInteger decision = [[NSUserDefaults standardUserDefaults] integerForKey:ContentFilterDecisionKey];
    switch (decision) {
    case (NSInteger)WebMockContentFilterDecisionAllow:
    case (NSInteger)WebMockContentFilterDecisionBlock:
        return (WebMockContentFilterDecision)decision;
    default:
        return WebMockContentFilterDecisionAllow;
    }
}

- (void)setContentFilteringDecisionPoint:(id)sender
{
    [[NSUserDefaults standardUserDefaults] setInteger:[sender tag] forKey:ContentFilterDecisionPointKey];
    [(BrowserAppDelegate *)[[NSApplication sharedApplication] delegate] didChangeSettings];
}

- (WebMockContentFilterDecisionPoint)contentFilteringDecisionPoint
{
    NSInteger decisionPoint = [[NSUserDefaults standardUserDefaults] integerForKey:ContentFilterDecisionPointKey];
    switch (decisionPoint) {
    case (NSInteger)WebMockContentFilterDecisionPointAfterWillSendRequest:
    case (NSInteger)WebMockContentFilterDecisionPointAfterRedirect:
    case (NSInteger)WebMockContentFilterDecisionPointAfterResponse:
    case (NSInteger)WebMockContentFilterDecisionPointAfterAddData:
    case (NSInteger)WebMockContentFilterDecisionPointAfterFinishedAddingData:
    case (NSInteger)WebMockContentFilterDecisionPointNever:
        return (WebMockContentFilterDecisionPoint)decisionPoint;
    default:
        return WebMockContentFilterDecisionPointAfterWillSendRequest;
    }
}

@end
