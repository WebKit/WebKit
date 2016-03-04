/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

static NSString * const defaultURL = @"http://www.webkit.org/";
static NSString * const DefaultURLPreferenceKey = @"DefaultURL";

static NSString * const UseWebKit2ByDefaultPreferenceKey = @"UseWebKit2ByDefault";
static NSString * const LayerBordersVisiblePreferenceKey = @"LayerBordersVisible";
static NSString * const SimpleLineLayoutDebugBordersEnabledPreferenceKey = @"SimpleLineLayoutDebugBordersEnabled";
static NSString * const TiledScrollingIndicatorVisiblePreferenceKey = @"TiledScrollingIndicatorVisible";
static NSString * const ResourceUsageOverlayVisiblePreferenceKey = @"ResourceUsageOverlayVisible";
static NSString * const IncrementalRenderingSuppressedPreferenceKey = @"IncrementalRenderingSuppressed";
static NSString * const AcceleratedDrawingEnabledPreferenceKey = @"AcceleratedDrawingEnabled";
static NSString * const DisplayListDrawingEnabledPreferenceKey = @"DisplayListDrawingEnabled";
static NSString * const ResourceLoadStatisticsEnabledPreferenceKey = @"ResourceLoadStatisticsEnabled";

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
    [self _addItemWithTitle:@"Enable Display List Drawing" action:@selector(toggleDisplayListDrawingEnabled:) indented:NO];
    [self _addItemWithTitle:@"Enable Resource Load Statistics" action:@selector(toggleResourceLoadStatisticsEnabled:) indented:NO];

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
    else if (action == @selector(toggleDisplayListDrawingEnabled:))
        [menuItem setState:[self displayListDrawingEnabled] ? NSOnState : NSOffState];
    else if (action == @selector(toggleResourceLoadStatisticsEnabled:))
        [menuItem setState:[self resourceLoadStatisticsEnabled] ? NSOnState : NSOffState];
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

- (void)toggleDisplayListDrawingEnabled:(id)sender
{
    [self _toggleBooleanDefault:DisplayListDrawingEnabledPreferenceKey];
}

- (BOOL)displayListDrawingEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:DisplayListDrawingEnabledPreferenceKey];
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

- (void)toggleResourceLoadStatisticsEnabled:(id)sender
{
    [self _toggleBooleanDefault:ResourceLoadStatisticsEnabledPreferenceKey];
}

- (BOOL)resourceLoadStatisticsEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:ResourceLoadStatisticsEnabledPreferenceKey];
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

@end
