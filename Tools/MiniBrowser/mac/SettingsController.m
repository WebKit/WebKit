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
#import "AppKitCompatibilityDeclarations.h"
#import "BrowserWindowController.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKInternalDebugFeature.h>

static NSString * const defaultURL = @"http://www.webkit.org/";
static NSString * const DefaultURLPreferenceKey = @"DefaultURL";

static NSString * const UseWebKit2ByDefaultPreferenceKey = @"UseWebKit2ByDefault";
static NSString * const CreateEditorByDefaultPreferenceKey = @"CreateEditorByDefault";
static NSString * const LayerBordersVisiblePreferenceKey = @"LayerBordersVisible";
static NSString * const SimpleLineLayoutEnabledPreferenceKey = @"SimpleLineLayoutEnabled";
static NSString * const SimpleLineLayoutDebugBordersEnabledPreferenceKey = @"SimpleLineLayoutDebugBordersEnabled";
static NSString * const TiledScrollingIndicatorVisiblePreferenceKey = @"TiledScrollingIndicatorVisible";
static NSString * const ReserveSpaceForBannersPreferenceKey = @"ReserveSpaceForBanners";

static NSString * const ResourceUsageOverlayVisiblePreferenceKey = @"ResourceUsageOverlayVisible";
static NSString * const LoadsAllSiteIconsKey = @"LoadsAllSiteIcons";
static NSString * const UsesGameControllerFrameworkKey = @"UsesGameControllerFramework";
static NSString * const IncrementalRenderingSuppressedPreferenceKey = @"IncrementalRenderingSuppressed";
static NSString * const AcceleratedDrawingEnabledPreferenceKey = @"AcceleratedDrawingEnabled";
static NSString * const DisplayListDrawingEnabledPreferenceKey = @"DisplayListDrawingEnabled";
static NSString * const SubpixelAntialiasedLayerTextEnabledPreferenceKey = @"SubpixelAntialiasedLayerTextEnabled";
static NSString * const ResourceLoadStatisticsEnabledPreferenceKey = @"ResourceLoadStatisticsEnabled";

static NSString * const NonFastScrollableRegionOverlayVisiblePreferenceKey = @"NonFastScrollableRegionOverlayVisible";
static NSString * const WheelEventHandlerRegionOverlayVisiblePreferenceKey = @"WheelEventHandlerRegionOverlayVisible";

static NSString * const UseTransparentWindowsPreferenceKey = @"UseTransparentWindows";
static NSString * const UsePaginatedModePreferenceKey = @"UsePaginatedMode";
static NSString * const EnableSubPixelCSSOMMetricsPreferenceKey = @"EnableSubPixelCSSOMMetrics";

static NSString * const LargeImageAsyncDecodingEnabledPreferenceKey = @"LargeImageAsyncDecodingEnabled";
static NSString * const AnimatedImageAsyncDecodingEnabledPreferenceKey = @"AnimatedImageAsyncDecodingEnabled";
static NSString * const AppleColorFilterEnabledPreferenceKey = @"AppleColorFilterEnabled";
static NSString * const PunchOutWhiteBackgroundsInDarkModePreferenceKey = @"PunchOutWhiteBackgroundsInDarkMode";
static NSString * const UseSystemAppearancePreferenceKey = @"UseSystemAppearance";

// This default name intentionally overlaps with the key that WebKit2 checks when creating a view.
static NSString * const UseRemoteLayerTreeDrawingAreaPreferenceKey = @"WebKit2UseRemoteLayerTreeDrawingArea";

static NSString * const PerWindowWebProcessesDisabledKey = @"PerWindowWebProcessesDisabled";
static NSString * const NetworkCacheSpeculativeRevalidationDisabledKey = @"NetworkCacheSpeculativeRevalidationDisabled";
static NSString * const ProcessSwapOnWindowOpenWithOpenerKey = @"ProcessSwapOnWindowOpenWithOpener";

typedef NS_ENUM(NSInteger, DebugOverylayMenuItemTag) {
    NonFastScrollableRegionOverlayTag = 100,
    WheelEventHandlerRegionOverlayTag,
    ExperimentalFeatureTag,
    InternalDebugFeatureTag,
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

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;

    NSArray *onByDefaultPrefs = @[
        UseWebKit2ByDefaultPreferenceKey,
        AcceleratedDrawingEnabledPreferenceKey,
        SimpleLineLayoutEnabledPreferenceKey,
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
        SubpixelAntialiasedLayerTextEnabledPreferenceKey,
#endif
        LargeImageAsyncDecodingEnabledPreferenceKey,
        AnimatedImageAsyncDecodingEnabledPreferenceKey,
    ];
    
    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
    for (NSString *prefName in onByDefaultPrefs) {
        if (![userDefaults objectForKey:prefName])
            [userDefaults setBool:YES forKey:prefName];
    }

    return self;
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
    [self _addItemWithTitle:@"Create Editor By Default" action:@selector(toggleCreateEditorByDefault:) indented:NO];
    [self _addItemWithTitle:@"Set Default URL to Current URL" action:@selector(setDefaultURLToCurrentURL:) indented:NO];

    [_menu addItem:[NSMenuItem separatorItem]];

    [self _addItemWithTitle:@"Use Transparent Windows" action:@selector(toggleUseTransparentWindows:) indented:NO];
    [self _addItemWithTitle:@"Use Paginated Mode" action:@selector(toggleUsePaginatedMode:) indented:NO];
    [self _addItemWithTitle:@"Show Layer Borders" action:@selector(toggleShowLayerBorders:) indented:NO];
    [self _addItemWithTitle:@"Disable Simple Line Layout" action:@selector(toggleSimpleLineLayoutEnabled:) indented:NO];
    [self _addItemWithTitle:@"Show Simple Line Layout Borders" action:@selector(toggleSimpleLineLayoutDebugBordersEnabled:) indented:NO];
    [self _addItemWithTitle:@"Suppress Incremental Rendering in New Windows" action:@selector(toggleIncrementalRenderingSuppressed:) indented:NO];
    [self _addItemWithTitle:@"Enable Accelerated Drawing" action:@selector(toggleAcceleratedDrawingEnabled:) indented:NO];
    [self _addItemWithTitle:@"Enable Display List Drawing" action:@selector(toggleDisplayListDrawingEnabled:) indented:NO];
    [self _addItemWithTitle:@"Enable Subpixel-antialiased Layer Text" action:@selector(toggleSubpixelAntialiasedLayerTextEnabled:) indented:NO];
    [self _addItemWithTitle:@"Enable Resource Load Statistics" action:@selector(toggleResourceLoadStatisticsEnabled:) indented:NO];
    [self _addItemWithTitle:@"Enable Large Image Async Decoding" action:@selector(toggleLargeImageAsyncDecodingEnabled:) indented:NO];
    [self _addItemWithTitle:@"Enable Animated Image Async Decoding" action:@selector(toggleAnimatedImageAsyncDecodingEnabled:) indented:NO];
    [self _addItemWithTitle:@"Enable color-filter" action:@selector(toggleAppleColorFilterEnabled:) indented:NO];
    [self _addItemWithTitle:@"Punch Out White Backgrounds in Dark Mode" action:@selector(togglePunchOutWhiteBackgroundsInDarkMode:) indented:NO];
    [self _addItemWithTitle:@"Use System Appearance" action:@selector(toggleUseSystemAppearance:) indented:NO];
    [self _addHeaderWithTitle:@"WebKit2-only Settings"];

    [self _addItemWithTitle:@"Reserve Space For Banners" action:@selector(toggleReserveSpaceForBanners:) indented:YES];
    [self _addItemWithTitle:@"Show Tiled Scrolling Indicator" action:@selector(toggleShowTiledScrollingIndicator:) indented:YES];
    [self _addItemWithTitle:@"Use UI-Side Compositing" action:@selector(toggleUseUISideCompositing:) indented:YES];
    [self _addItemWithTitle:@"Disable Per-Window Web Processes" action:@selector(togglePerWindowWebProcessesDisabled:) indented:YES];
    [self _addItemWithTitle:@"Show Resource Usage Overlay" action:@selector(toggleShowResourceUsageOverlay:) indented:YES];
    [self _addItemWithTitle:@"Load All Site Icons Per-Page" action:@selector(toggleLoadsAllSiteIcons:) indented:YES];
    [self _addItemWithTitle:@"Use GameController.framework on macOS (Restart required)" action:@selector(toggleUsesGameControllerFramework:) indented:YES];
    [self _addItemWithTitle:@"Disable network cache speculative revalidation" action:@selector(toggleNetworkCacheSpeculativeRevalidationDisabled:) indented:YES];
    [self _addItemWithTitle:@"Enable Process Swap on window.open() with an opener" action:@selector(toggleProcessSwapOnWindowOpenWithOpener:) indented:YES];

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

    NSMenuItem *experimentalFeaturesSubmenuItem = [[NSMenuItem alloc] initWithTitle:@"Experimental Features" action:nil keyEquivalent:@""];
    NSMenu *experimentalFeaturesMenu = [[NSMenu alloc] initWithTitle:@"Experimental Features"];
    [experimentalFeaturesSubmenuItem setSubmenu:experimentalFeaturesMenu];

    NSArray<_WKExperimentalFeature *> *experimentalFeatures = [WKPreferences _experimentalFeatures];

    for (_WKExperimentalFeature *feature in experimentalFeatures) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:feature.name action:@selector(toggleExperimentalFeature:) keyEquivalent:@""];
        item.toolTip = feature.details;
        item.representedObject = feature;

        [item setTag:ExperimentalFeatureTag];
        [item setTarget:self];
        [experimentalFeaturesMenu addItem:[item autorelease]];
    }

    [_menu addItem:experimentalFeaturesSubmenuItem];
    [experimentalFeaturesSubmenuItem release];
    [experimentalFeaturesMenu release];

    NSMenuItem *internalDebugFeaturesSubmenuItem = [[NSMenuItem alloc] initWithTitle:@"Internal Features" action:nil keyEquivalent:@""];
    NSMenu *internalDebugFeaturesMenu = [[NSMenu alloc] initWithTitle:@"Internal Features"];
    [internalDebugFeaturesSubmenuItem setSubmenu:internalDebugFeaturesMenu];

    NSArray<_WKInternalDebugFeature *> *internalDebugFeatures = [WKPreferences _internalDebugFeatures];

    for (_WKInternalDebugFeature *feature in internalDebugFeatures) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:feature.name action:@selector(toggleInternalDebugFeature:) keyEquivalent:@""];
        item.toolTip = feature.details;
        item.representedObject = feature;

        [item setTag:InternalDebugFeatureTag];
        [item setTarget:self];
        [internalDebugFeaturesMenu addItem:[item autorelease]];
    }

    [_menu addItem:internalDebugFeaturesSubmenuItem];
    [internalDebugFeaturesSubmenuItem release];
    [internalDebugFeaturesMenu release];

    [self _addHeaderWithTitle:@"WebKit1-only Settings"];
    [self _addItemWithTitle:@"Enable Subpixel CSSOM Metrics" action:@selector(toggleEnableSubPixelCSSOMMetrics:) indented:YES];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL action = [menuItem action];

    if (action == @selector(toggleUseWebKit2ByDefault:))
        [menuItem setState:[self useWebKit2ByDefault] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleCreateEditorByDefault:))
        [menuItem setState:[self createEditorByDefault] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleUseTransparentWindows:))
        [menuItem setState:[self useTransparentWindows] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleUsePaginatedMode:))
        [menuItem setState:[self usePaginatedMode] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleShowLayerBorders:))
        [menuItem setState:[self layerBordersVisible] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleSimpleLineLayoutEnabled:))
        [menuItem setState:[self simpleLineLayoutEnabled] ? NSControlStateValueOff : NSControlStateValueOn];
    else if (action == @selector(toggleSimpleLineLayoutDebugBordersEnabled:))
        [menuItem setState:[self simpleLineLayoutDebugBordersEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleIncrementalRenderingSuppressed:))
        [menuItem setState:[self incrementalRenderingSuppressed] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleAcceleratedDrawingEnabled:))
        [menuItem setState:[self acceleratedDrawingEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleDisplayListDrawingEnabled:))
        [menuItem setState:[self displayListDrawingEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleSubpixelAntialiasedLayerTextEnabled:))
        [menuItem setState:[self subpixelAntialiasedLayerTextEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleResourceLoadStatisticsEnabled:))
        [menuItem setState:[self resourceLoadStatisticsEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleLargeImageAsyncDecodingEnabled:))
        [menuItem setState:[self largeImageAsyncDecodingEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleAnimatedImageAsyncDecodingEnabled:))
        [menuItem setState:[self animatedImageAsyncDecodingEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleAppleColorFilterEnabled:))
        [menuItem setState:[self appleColorFilterEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(togglePunchOutWhiteBackgroundsInDarkMode:))
        [menuItem setState:[self punchOutWhiteBackgroundsInDarkMode] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleUseSystemAppearance:))
        [menuItem setState:[self useSystemAppearance] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleReserveSpaceForBanners:))
        [menuItem setState:[self isSpaceReservedForBanners] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleShowTiledScrollingIndicator:))
        [menuItem setState:[self tiledScrollingIndicatorVisible] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleShowResourceUsageOverlay:))
        [menuItem setState:[self resourceUsageOverlayVisible] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleLoadsAllSiteIcons:))
        [menuItem setState:[self loadsAllSiteIcons] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleUsesGameControllerFramework:))
        [menuItem setState:[self usesGameControllerFramework] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleNetworkCacheSpeculativeRevalidationDisabled:))
        [menuItem setState:[self networkCacheSpeculativeRevalidationDisabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleProcessSwapOnWindowOpenWithOpener:))
        [menuItem setState:[self processSwapOnWindowOpenWithOpenerEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleUseUISideCompositing:))
        [menuItem setState:[self useUISideCompositing] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(togglePerWindowWebProcessesDisabled:))
        [menuItem setState:[self perWindowWebProcessesDisabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleEnableSubPixelCSSOMMetrics:))
        [menuItem setState:[self subPixelCSSOMMetricsEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleDebugOverlay:))
        [menuItem setState:[self debugOverlayVisible:menuItem] ? NSControlStateValueOn : NSControlStateValueOff];

    if (menuItem.tag == ExperimentalFeatureTag) {
        _WKExperimentalFeature *feature = menuItem.representedObject;
        [menuItem setState:[defaultPreferences() _isEnabledForExperimentalFeature:feature] ? NSControlStateValueOn : NSControlStateValueOff];
    }
    if (menuItem.tag == InternalDebugFeatureTag) {
        _WKInternalDebugFeature *feature = menuItem.representedObject;
        [menuItem setState:[defaultPreferences() _isEnabledForInternalDebugFeature:feature] ? NSControlStateValueOn : NSControlStateValueOff];
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

- (void)toggleCreateEditorByDefault:(id)sender
{
    [self _toggleBooleanDefault:CreateEditorByDefaultPreferenceKey];
}

- (BOOL)createEditorByDefault
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:CreateEditorByDefaultPreferenceKey];
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

    NSModalResponse response = [alert runModal];    
    [alert release];

    if (response != NSAlertFirstButtonReturn)
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

- (void)toggleSimpleLineLayoutEnabled:(id)sender
{
    [self _toggleBooleanDefault:SimpleLineLayoutEnabledPreferenceKey];
}

- (BOOL)simpleLineLayoutEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:SimpleLineLayoutEnabledPreferenceKey];
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

- (void)toggleSubpixelAntialiasedLayerTextEnabled:(id)sender
{
    [self _toggleBooleanDefault:SubpixelAntialiasedLayerTextEnabledPreferenceKey];
}

- (BOOL)subpixelAntialiasedLayerTextEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:SubpixelAntialiasedLayerTextEnabledPreferenceKey];
}

- (void)toggleReserveSpaceForBanners:(id)sender
{
    [self _toggleBooleanDefault:ReserveSpaceForBannersPreferenceKey];
}

- (void)toggleShowTiledScrollingIndicator:(id)sender
{
    [self _toggleBooleanDefault:TiledScrollingIndicatorVisiblePreferenceKey];
}

- (void)toggleShowResourceUsageOverlay:(id)sender
{
    [self _toggleBooleanDefault:ResourceUsageOverlayVisiblePreferenceKey];
}

- (BOOL)loadsAllSiteIcons
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:LoadsAllSiteIconsKey];
}

- (void)toggleLoadsAllSiteIcons:(id)sender
{
    [self _toggleBooleanDefault:LoadsAllSiteIconsKey];
}

- (BOOL)usesGameControllerFramework
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:UsesGameControllerFrameworkKey];
}

- (void)toggleUsesGameControllerFramework:(id)sender
{
    [self _toggleBooleanDefault:UsesGameControllerFrameworkKey];
}

- (BOOL)networkCacheSpeculativeRevalidationDisabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:NetworkCacheSpeculativeRevalidationDisabledKey];
}

- (void)toggleNetworkCacheSpeculativeRevalidationDisabled:(id)sender
{
    [self _toggleBooleanDefault:NetworkCacheSpeculativeRevalidationDisabledKey];
}

- (BOOL)processSwapOnWindowOpenWithOpenerEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:ProcessSwapOnWindowOpenWithOpenerKey];
}

- (void)toggleProcessSwapOnWindowOpenWithOpener:(id)sender
{
    [self _toggleBooleanDefault:ProcessSwapOnWindowOpenWithOpenerKey];
}

- (BOOL)isSpaceReservedForBanners
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:ReserveSpaceForBannersPreferenceKey];
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

- (void)toggleLargeImageAsyncDecodingEnabled:(id)sender
{
    [self _toggleBooleanDefault:LargeImageAsyncDecodingEnabledPreferenceKey];
}

- (BOOL)largeImageAsyncDecodingEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:LargeImageAsyncDecodingEnabledPreferenceKey];
}

- (void)toggleAnimatedImageAsyncDecodingEnabled:(id)sender
{
    [self _toggleBooleanDefault:AnimatedImageAsyncDecodingEnabledPreferenceKey];
}

- (BOOL)animatedImageAsyncDecodingEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:AnimatedImageAsyncDecodingEnabledPreferenceKey];
}

- (void)toggleAppleColorFilterEnabled:(id)sender
{
    [self _toggleBooleanDefault:AppleColorFilterEnabledPreferenceKey];
}

- (BOOL)appleColorFilterEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:AppleColorFilterEnabledPreferenceKey];
}

- (void)togglePunchOutWhiteBackgroundsInDarkMode:(id)sender
{
    [self _toggleBooleanDefault:PunchOutWhiteBackgroundsInDarkModePreferenceKey];
}

- (BOOL)punchOutWhiteBackgroundsInDarkMode
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:PunchOutWhiteBackgroundsInDarkModePreferenceKey];
}

- (void)toggleUseSystemAppearance:(id)sender
{
    [self _toggleBooleanDefault:UseSystemAppearancePreferenceKey];
}

- (BOOL)useSystemAppearance
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:UseSystemAppearancePreferenceKey];
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

- (void)toggleExperimentalFeature:(id)sender
{
    _WKExperimentalFeature *feature = ((NSMenuItem *)sender).representedObject;
    WKPreferences *preferences = defaultPreferences();

    BOOL currentlyEnabled = [preferences _isEnabledForExperimentalFeature:feature];
    [preferences _setEnabled:!currentlyEnabled forExperimentalFeature:feature];

    [[NSUserDefaults standardUserDefaults] setBool:!currentlyEnabled forKey:feature.key];
}

- (void)toggleInternalDebugFeature:(id)sender
{
    _WKInternalDebugFeature *feature = ((NSMenuItem *)sender).representedObject;
    WKPreferences *preferences = defaultPreferences();

    BOOL currentlyEnabled = [preferences _isEnabledForInternalDebugFeature:feature];
    [preferences _setEnabled:!currentlyEnabled forInternalDebugFeature:feature];

    [[NSUserDefaults standardUserDefaults] setBool:!currentlyEnabled forKey:feature.key];
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
