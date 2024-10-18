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
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKInternalDebugFeature.h>

NSString * const kUserAgentChangedNotificationName = @"UserAgentChangedNotification";

static NSString * const defaultURL = @"http://www.webkit.org/";
static NSString * const DefaultURLPreferenceKey = @"DefaultURL";

static NSString * const CustomUserAgentPreferenceKey = @"CustomUserAgentIdentifier";

static NSString * const UseWebKit2ByDefaultPreferenceKey = @"UseWebKit2ByDefault";
static NSString * const CreateEditorByDefaultPreferenceKey = @"CreateEditorByDefault";
static NSString * const LayerBordersVisiblePreferenceKey = @"LayerBordersVisible";
static NSString * const LegacyLineLayoutVisualCoverageEnabledPreferenceKey = @"LegacyLineLayoutVisualCoverageEnabled";
static NSString * const TiledScrollingIndicatorVisiblePreferenceKey = @"TiledScrollingIndicatorVisible";
static NSString * const ReserveSpaceForBannersPreferenceKey = @"ReserveSpaceForBanners";
static NSString * const WebViewFillsWindowKey = @"WebViewFillsWindow";

static NSString * const ResourceUsageOverlayVisiblePreferenceKey = @"ResourceUsageOverlayVisible";
static NSString * const LoadsAllSiteIconsKey = @"LoadsAllSiteIcons";
static NSString * const UsesGameControllerFrameworkKey = @"UsesGameControllerFramework";
static NSString * const IncrementalRenderingSuppressedPreferenceKey = @"IncrementalRenderingSuppressed";
static NSString * const AcceleratedDrawingEnabledPreferenceKey = @"AcceleratedDrawingEnabled";
static NSString * const ResourceLoadStatisticsEnabledPreferenceKey = @"ResourceLoadStatisticsEnabled";

static NSString * const NonFastScrollableRegionOverlayVisiblePreferenceKey = @"NonFastScrollableRegionOverlayVisible";
static NSString * const WheelEventHandlerRegionOverlayVisiblePreferenceKey = @"WheelEventHandlerRegionOverlayVisible";
static NSString * const InteractionRegionOverlayVisiblePreferenceKey = @"InteractionRegionOverlayVisible";

static NSString * const UseTransparentWindowsPreferenceKey = @"UseTransparentWindows";
static NSString * const UsePaginatedModePreferenceKey = @"UsePaginatedMode";

static NSString * const LargeImageAsyncDecodingEnabledPreferenceKey = @"LargeImageAsyncDecodingEnabled";
static NSString * const AnimatedImageAsyncDecodingEnabledPreferenceKey = @"AnimatedImageAsyncDecodingEnabled";
static NSString * const AppleColorFilterEnabledPreferenceKey = @"AppleColorFilterEnabled";
static NSString * const SiteSpecificQuirksModeEnabledPreferenceKey = @"SiteSpecificQuirksModeEnabled";
static NSString * const PunchOutWhiteBackgroundsInDarkModePreferenceKey = @"PunchOutWhiteBackgroundsInDarkMode";
static NSString * const UseSystemAppearancePreferenceKey = @"UseSystemAppearance";
static NSString * const DataDetectorsEnabledPreferenceKey = @"DataDetectorsEnabled";
static NSString * const UseMockCaptureDevicesPreferenceKey = @"UseMockCaptureDevices";
static NSString * const AttachmentElementEnabledPreferenceKey = @"AttachmentElementEnabled";
static NSString * const AdvancedPrivacyProtectionsPreferenceKey = @"AdvancedPrivacyProtectionsEnabled";
static NSString * const AllowsContentJavascriptPreferenceKey = @"AllowsContentJavascript";

static NSString * const SiteIsolationOverlayPreferenceKey = @"SiteIsolationOverlayEnabled";

// This default name intentionally overlaps with the key that WebKit2 checks when creating a view.
static NSString * const UseRemoteLayerTreeDrawingAreaPreferenceKey = @"WebKit2UseRemoteLayerTreeDrawingArea";

static NSString * const PerWindowWebProcessesDisabledKey = @"PerWindowWebProcessesDisabled";
static NSString * const NetworkCacheSpeculativeRevalidationDisabledKey = @"NetworkCacheSpeculativeRevalidationDisabled";

typedef NS_ENUM(NSInteger, DebugOverylayMenuItemTag) {
    NonFastScrollableRegionOverlayTag = 100,
    WheelEventHandlerRegionOverlayTag,
    InteractionRegionOverlayTag,
    ExperimentalFeatureTag,
    InternalDebugFeatureTag,
    SiteIsolationRegionOverlayTag,
};

typedef NS_ENUM(NSInteger, AttachmentElementEnabledMenuItemTag) {
    AttachmentElementDisabledTag = 100,
    AttachmentElementEnabledTag,
    AttachmentElementWideLayoutEnabledTag,
};

@implementation SettingsController

- (instancetype)initWithMenu:(NSMenu *)menu
{
    self = [super init];
    if (!self)
        return nil;

    NSArray *onByDefaultPrefs = @[
        UseWebKit2ByDefaultPreferenceKey,
        AcceleratedDrawingEnabledPreferenceKey,
        LargeImageAsyncDecodingEnabledPreferenceKey,
        AnimatedImageAsyncDecodingEnabledPreferenceKey,
        SiteSpecificQuirksModeEnabledPreferenceKey,
        WebViewFillsWindowKey,
        ResourceLoadStatisticsEnabledPreferenceKey,
        AllowsContentJavascriptPreferenceKey,
    ];

    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
    for (NSString *prefName in onByDefaultPrefs) {
        if (![userDefaults objectForKey:prefName])
            [userDefaults setBool:YES forKey:prefName];
    }

    [self _populateMenu:menu];

    return self;
}

static NSMenuItem *addItemToMenuWithTarget(NSMenu *menu, NSString *title, id target, SEL action, BOOL indent, NSInteger tag)
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:@""];
    if (action)
        item.target = target;
    if (tag)
        item.tag = tag;
    if (indent)
        item.indentationLevel = 1;
    [menu addItem:item];
    return item;
}

static void addSeparatorToMenu(NSMenu *menu)
{
    [menu addItem:[NSMenuItem separatorItem]];
}

static NSMenu *addSubmenuToMenu(NSMenu *menu, NSString *title)
{
    NSMenuItem *submenuItem = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
    NSMenu *submenu = [[NSMenu alloc] initWithTitle:title];
    [submenuItem setSubmenu:submenu];
    [menu addItem:submenuItem];
    return submenu;
}

- (void)_populateMenu:(NSMenu *)menu
{
    __block bool indent = false;

    __auto_type addItemToMenu = ^(NSMenu *menu, NSString *title, SEL action, BOOL indent, NSInteger tag) {
        return addItemToMenuWithTarget(menu, title, self, action, indent, tag);
    };

    __auto_type addItem = ^(NSString *title, SEL action) {
        return addItemToMenu(menu, title, action, indent, 0);
    };

    __auto_type addSeparator = ^{
        addSeparatorToMenu(menu);
    };

    __auto_type addSubmenu = ^(NSString *title) {
        return addSubmenuToMenu(menu, title);
    };

    addItem(@"Use WebKit2 By Default", @selector(toggleUseWebKit2ByDefault:));
    addItem(@"Create Editor By Default", @selector(toggleCreateEditorByDefault:));
    addItem(@"Set Default URL to Current URL", @selector(setDefaultURLToCurrentURL:));

    addSeparator();

    NSMenu *userAgentMenu = addSubmenu(@"User Agent");
    [self buildUserAgentsMenu:userAgentMenu];

    addSeparator();

    addItem(@"Use Transparent Windows", @selector(toggleUseTransparentWindows:));
    addItem(@"Use Paginated Mode", @selector(toggleUsePaginatedMode:));
    addItem(@"Show Layer Borders", @selector(toggleShowLayerBorders:));
    addItem(@"Enable Legacy Line Layout Visual Coverage", @selector(toggleLegacyLineLayoutVisualCoverageEnabled:));
    addItem(@"Suppress Incremental Rendering in New Windows", @selector(toggleIncrementalRenderingSuppressed:));
    addItem(@"Enable Accelerated Drawing", @selector(toggleAcceleratedDrawingEnabled:));
    addItem(@"Enable Resource Load Statistics", @selector(toggleResourceLoadStatisticsEnabled:));
    addItem(@"Enable Large Image Async Decoding", @selector(toggleLargeImageAsyncDecodingEnabled:));
    addItem(@"Enable Animated Image Async Decoding", @selector(toggleAnimatedImageAsyncDecodingEnabled:));
    addItem(@"Enable color-filter", @selector(toggleAppleColorFilterEnabled:));
    addItem(@"Enable site-specific quirks", @selector(toggleSiteSpecificQuirksModeEnabled:));
    addItem(@"Punch Out White Backgrounds in Dark Mode", @selector(togglePunchOutWhiteBackgroundsInDarkMode:));
    addItem(@"Use System Appearance", @selector(toggleUseSystemAppearance:));
    addItem(@"Enable Data Detectors", @selector(toggleDataDetectorsEnabled:));
    addItem(@"Use Mock Capture Devices", @selector(toggleUseMockCaptureDevices:));
    addItem(@"Advanced Privacy Protections", @selector(toggleAdvancedPrivacyProtections:));

    NSMenu *attachmentElementMenu = addSubmenu(@"Enable Attachment Element");
    addItemToMenu(attachmentElementMenu, @"Disabled", @selector(changeAttachmentElementEnabled:), NO, AttachmentElementDisabledTag);
    addItemToMenu(attachmentElementMenu, @"Enable with default layout", @selector(changeAttachmentElementEnabled:), NO, AttachmentElementEnabledTag);
    addItemToMenu(attachmentElementMenu, @"Enable with wide layout", @selector(changeAttachmentElementEnabled:), NO, AttachmentElementWideLayoutEnabledTag);

    addSeparator();
    addItem(@"WebKit2-only Settings", nil);
    indent = YES;
    addItem(@"Reserve Space For Banners", @selector(toggleReserveSpaceForBanners:));
    addItem(@"Show Tiled Scrolling Indicator", @selector(toggleShowTiledScrollingIndicator:));
    addItem(@"Use UI-Side Compositing", @selector(toggleUseUISideCompositing:));
    addItem(@"Disable Per-Window Web Processes", @selector(togglePerWindowWebProcessesDisabled:));
    addItem(@"Load All Site Icons Per-Page", @selector(toggleLoadsAllSiteIcons:));
    addItem(@"Use GameController.framework on macOS (Restart required)", @selector(toggleUsesGameControllerFramework:));
    addItem(@"Disable network cache speculative revalidation", @selector(toggleNetworkCacheSpeculativeRevalidationDisabled:));
    addItem(@"Allow JavaScript from web content to run", @selector(toggleAllowsContentJavascript:));
    indent = NO;

    NSMenu *debugOverlaysMenu = addSubmenu(@"Debug Overlays");
    addItemToMenu(debugOverlaysMenu, @"Non-fast Scrollable Region", @selector(toggleDebugOverlay:), NO, NonFastScrollableRegionOverlayTag);
    addItemToMenu(debugOverlaysMenu, @"Wheel Event Handler Region", @selector(toggleDebugOverlay:), NO, WheelEventHandlerRegionOverlayTag);
    addItemToMenu(debugOverlaysMenu, @"Interaction Region", @selector(toggleDebugOverlay:), NO, InteractionRegionOverlayTag);
    addItemToMenu(debugOverlaysMenu, @"Resource Usage", @selector(toggleShowResourceUsageOverlay:), NO, 0);
    addItemToMenu(debugOverlaysMenu, @"Site Isolation", @selector(toggleSiteIsolationOverlay:), NO, SiteIsolationRegionOverlayTag);

    NSMenu *experimentalFeaturesMenu = addSubmenu(@"Experimental Features");
    for (_WKExperimentalFeature *feature in WKPreferences._experimentalFeatures) {
        NSMenuItem *item = addItemToMenu(experimentalFeaturesMenu, feature.name, @selector(toggleExperimentalFeature:), NO, ExperimentalFeatureTag);
        item.toolTip = feature.details;
        item.representedObject = feature;
    }
    addSeparatorToMenu(experimentalFeaturesMenu);
    addItemToMenu(experimentalFeaturesMenu, @"Reset All to Defaults", @selector(resetAllExperimentalFeatures:), NO, 0);

    NSMenu *internalDebugFeaturesMenu = addSubmenu(@"Internal Features");
    for (_WKFeature *feature in WKPreferences._internalDebugFeatures) {
        NSMenuItem *item = addItemToMenu(internalDebugFeaturesMenu, feature.name, @selector(toggleInternalDebugFeature:), NO, InternalDebugFeatureTag);
        item.toolTip = feature.details;
        item.representedObject = feature;
    }
    addSeparatorToMenu(internalDebugFeaturesMenu);
    addItemToMenu(internalDebugFeaturesMenu, @"Reset All to Defaults", @selector(resetAllInternalDebugFeatures:), NO, 0);
}

+ (NSArray *)userAgentData
{
    return @[
        @{
            @"label" : @"Safari 16.4",
            @"identifier" : @"safari",
            @"userAgent" : @"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.4 Safari/605.1.15"
        },
        @{
            @"label" : @"-",
        },
        @{
            @"label" : @"Safari—iOS 16.4—iPhone",
            @"identifier" : @"iphone-safari",
            @"userAgent" : @"Mozilla/5.0 (iPhone; CPU iPhone OS 16_4 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.4 Mobile/15E148 Safari/604.1"
        },
        @{
            @"label" : @"Safari—iPadOS 16.4—iPad mini",
            @"identifier" : @"ipad-mini-safari",
            @"userAgent" : @"Mozilla/5.0 (iPad; CPU iPhone OS 16_4 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.4 Mobile/15E148 Safari/604.1"
        },
        @{
            @"label" : @"Safari—iPadOS 16.4—iPad",
            @"identifier" : @"ipad-safari",
            @"userAgent" : @"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_6) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.4 Safari/605.1.15"
        },
        @{
            @"label" : @"-",
        },
        @{
            @"label" : @"Firefox—macOS",
            @"identifier" : @"firefox",
            @"userAgent" : @"Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:109.0) Gecko/20100101 Firefox/109.0"
        },
        @{
            @"label" : @"Firefox—Windows",
            @"identifier" : @"windows-firefox",
            @"userAgent" : @"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/109.0"
        },
        @{
            @"label" : @"-",
        },
        @{
            @"label" : @"Chrome—macOS",
            @"identifier" : @"chrome",
            @"userAgent" : @"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/110.0.0.0 Safari/537.36"
        },
        @{
            @"label" : @"Chrome—Windows",
            @"identifier" : @"windows-chrome",
            @"userAgent" : @"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/110.0.0.0 Safari/537.36"
        },
        @{
            @"label" : @"Chrome—Android",
            @"identifier" : @"android-chrome",
            @"userAgent" : @"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/110.0.0.0 Safari/537.36"
        },
        @{
            @"label" : @"-",
        },
        @{
            @"label" : @"Edge—macOS",
            @"identifier" : @"edge",
            @"userAgent" : @"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/110.0.0.0 Safari/537.36 Edg/110.0.1587.41"
        },
        @{
            @"label" : @"Edge—Windows",
            @"identifier" : @"windows-edge",
            @"userAgent" : @"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/110.0.0.0 Safari/537.36 Edg/110.0.1587.41"
        },
    ];
}

- (void)buildUserAgentsMenu:(NSMenu *)menu
{
    NSDictionary* defaultUAInfo = @{
        @"label" : @"Default",
        @"identifier" : @"default",
    };

    __auto_type addItem = ^(NSString *title, SEL action) {
        return addItemToMenuWithTarget(menu, title, self, action, NO, 0);
    };

    __auto_type addSeparator = ^{
        addSeparatorToMenu(menu);
    };

    NSMenuItem *defaultItem = addItem(@"Default", @selector(changeCustomUserAgent:));
    defaultItem.representedObject = defaultUAInfo;

    addSeparator();

    for (NSDictionary *userAgentData in SettingsController.userAgentData) {
        NSString *name = userAgentData[@"label"];

        if ([name isEqualToString:@"-"]) {
            addSeparator();
            continue;
        }

        NSMenuItem *item = addItem(name, @selector(changeCustomUserAgent:));
        item.representedObject = userAgentData;
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-implementations"
- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
#pragma GCC diagnostic pop
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
    else if (action == @selector(toggleLegacyLineLayoutVisualCoverageEnabled:))
        [menuItem setState:[self legacyLineLayoutVisualCoverageEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleIncrementalRenderingSuppressed:))
        [menuItem setState:[self incrementalRenderingSuppressed] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleAcceleratedDrawingEnabled:))
        [menuItem setState:[self acceleratedDrawingEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleResourceLoadStatisticsEnabled:))
        [menuItem setState:[self resourceLoadStatisticsEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleLargeImageAsyncDecodingEnabled:))
        [menuItem setState:[self largeImageAsyncDecodingEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleAnimatedImageAsyncDecodingEnabled:))
        [menuItem setState:[self animatedImageAsyncDecodingEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleAppleColorFilterEnabled:))
        [menuItem setState:[self appleColorFilterEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleSiteSpecificQuirksModeEnabled:))
        [menuItem setState:[self siteSpecificQuirksModeEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(togglePunchOutWhiteBackgroundsInDarkMode:))
        [menuItem setState:[self punchOutWhiteBackgroundsInDarkMode] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleUseSystemAppearance:))
        [menuItem setState:[self useSystemAppearance] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleDataDetectorsEnabled:))
        [menuItem setState:[self dataDetectorsEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleUseMockCaptureDevices:))
        [menuItem setState:[self useMockCaptureDevices] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleAdvancedPrivacyProtections:))
        [menuItem setState:[self advancedPrivacyProtectionsEnabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleAllowsContentJavascript:))
        [menuItem setState:[self allowsContentJavascript] ? NSControlStateValueOn : NSControlStateValueOff];
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
    else if (action == @selector(toggleUseUISideCompositing:))
        [menuItem setState:[self useUISideCompositing] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(togglePerWindowWebProcessesDisabled:))
        [menuItem setState:[self perWindowWebProcessesDisabled] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleDebugOverlay:))
        [menuItem setState:[self debugOverlayVisible:menuItem] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleSiteIsolationOverlay:))
        [menuItem setState:[self siteIsolationOverlayVisible:menuItem] ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(changeCustomUserAgent:)) {

        NSString *savedUAIdentifier = [[NSUserDefaults standardUserDefaults] stringForKey:CustomUserAgentPreferenceKey];
        NSDictionary *userAgentDict = [menuItem representedObject];
        if (userAgentDict) {
            BOOL isCurrentUA = [userAgentDict[@"identifier"] isEqualToString:savedUAIdentifier];
            [menuItem setState:isCurrentUA ? NSControlStateValueOn : NSControlStateValueOff];
        } else
            [menuItem setState:NSControlStateValueOff];
    } else if (action == @selector(changeAttachmentElementEnabled:))
        [menuItem setState:[self attachmentElementEnabled:menuItem] ? NSControlStateValueOn : NSControlStateValueOff];

    WKPreferences *defaultPreferences = [[NSApplication sharedApplication] browserAppDelegate].defaultPreferences;
    if (menuItem.tag == ExperimentalFeatureTag) {
        _WKFeature *feature = menuItem.representedObject;
        [menuItem setState:[defaultPreferences _isEnabledForFeature:feature] ? NSControlStateValueOn : NSControlStateValueOff];
    }
    if (menuItem.tag == InternalDebugFeatureTag) {
        _WKFeature *feature = menuItem.representedObject;
        [menuItem setState:[defaultPreferences _isEnabledForFeature:feature] ? NSControlStateValueOn : NSControlStateValueOff];
    }

    return YES;
}

- (void)_toggleBooleanDefault:(NSString *)defaultName
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setBool:![defaults boolForKey:defaultName] forKey:defaultName];

    [[[NSApplication sharedApplication] browserAppDelegate] didChangeSettings];
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
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 140000
    bool useRemoteLayerTree = true;
#else
    bool useRemoteLayerTree = false;
#endif
    id useRemoteLayerTreeBoolean = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKit2UseRemoteLayerTreeDrawingArea"];
    if (useRemoteLayerTreeBoolean)
        useRemoteLayerTree = [useRemoteLayerTreeBoolean boolValue];
    return useRemoteLayerTree;
}

- (void)togglePerWindowWebProcessesDisabled:(id)sender
{
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:self.perWindowWebProcessesDisabled ? @"Are you sure you want to switch to per-window web processes?" : @"Are you sure you want to switch to a single web process?"];
    [alert setInformativeText:@"This requires quitting and relaunching MiniBrowser. I'll do the quitting. You will have to do the relaunching."];
    [alert addButtonWithTitle:@"Switch and Quit"];
    [alert addButtonWithTitle:@"Cancel"];

    NSModalResponse response = [alert runModal];
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

- (void)toggleLegacyLineLayoutVisualCoverageEnabled:(id)sender
{
    [self _toggleBooleanDefault:LegacyLineLayoutVisualCoverageEnabledPreferenceKey];
}

- (BOOL)legacyLineLayoutVisualCoverageEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:LegacyLineLayoutVisualCoverageEnabledPreferenceKey];
}

- (void)toggleAcceleratedDrawingEnabled:(id)sender
{
    [self _toggleBooleanDefault:AcceleratedDrawingEnabledPreferenceKey];
}

- (BOOL)acceleratedDrawingEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:AcceleratedDrawingEnabledPreferenceKey];
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

- (BOOL)isSpaceReservedForBanners
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:ReserveSpaceForBannersPreferenceKey];
}

- (BOOL)webViewFillsWindow
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebViewFillsWindowKey];
}

- (void)setWebViewFillsWindow:(BOOL)fillsWindow
{
    return [[NSUserDefaults standardUserDefaults] setBool:fillsWindow forKey:WebViewFillsWindowKey];
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

- (void)toggleSiteSpecificQuirksModeEnabled:(id)sender
{
    [self _toggleBooleanDefault:SiteSpecificQuirksModeEnabledPreferenceKey];
}

- (BOOL)siteSpecificQuirksModeEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:SiteSpecificQuirksModeEnabledPreferenceKey];
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

- (void)toggleDataDetectorsEnabled:(id)sender
{
    [self _toggleBooleanDefault:DataDetectorsEnabledPreferenceKey];
}

- (BOOL)dataDetectorsEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:DataDetectorsEnabledPreferenceKey];
}

- (void)toggleUseMockCaptureDevices:(id)sender
{
    [self _toggleBooleanDefault:UseMockCaptureDevicesPreferenceKey];
}

- (BOOL)useMockCaptureDevices
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:UseMockCaptureDevicesPreferenceKey];
}

- (void)toggleAdvancedPrivacyProtections:(id)sender
{
    [self _toggleBooleanDefault:AdvancedPrivacyProtectionsPreferenceKey];
}

- (BOOL)advancedPrivacyProtectionsEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:AdvancedPrivacyProtectionsPreferenceKey];
}

- (void)toggleAllowsContentJavascript:(id)sender
{
    [self _toggleBooleanDefault:AllowsContentJavascriptPreferenceKey];
}

- (BOOL)allowsContentJavascript
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:AllowsContentJavascriptPreferenceKey];
}

- (BOOL)nonFastScrollableRegionOverlayVisible
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:NonFastScrollableRegionOverlayVisiblePreferenceKey];
}

- (BOOL)wheelEventHandlerRegionOverlayVisible
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WheelEventHandlerRegionOverlayVisiblePreferenceKey];
}

- (BOOL)interactionRegionOverlayVisible
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:InteractionRegionOverlayVisiblePreferenceKey];
}

- (NSString *)preferenceKeyForRegionOverlayTag:(NSUInteger)tag
{
    switch (tag) {
    case NonFastScrollableRegionOverlayTag:
        return NonFastScrollableRegionOverlayVisiblePreferenceKey;

    case WheelEventHandlerRegionOverlayTag:
        return WheelEventHandlerRegionOverlayVisiblePreferenceKey;

    case InteractionRegionOverlayTag:
        return InteractionRegionOverlayVisiblePreferenceKey;

    case SiteIsolationRegionOverlayTag:
        return SiteIsolationOverlayPreferenceKey;
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
    _WKFeature *feature = ((NSMenuItem *)sender).representedObject;
    WKPreferences *preferences = [[NSApplication sharedApplication] browserAppDelegate].defaultPreferences;

    BOOL currentlyEnabled = [preferences _isEnabledForFeature:feature];
    [preferences _setEnabled:!currentlyEnabled forFeature:feature];

    [[NSUserDefaults standardUserDefaults] setBool:!currentlyEnabled forKey:feature.key];
}

- (void)toggleInternalDebugFeature:(id)sender
{
    _WKInternalDebugFeature *feature = ((NSMenuItem *)sender).representedObject;
    WKPreferences *preferences = [[NSApplication sharedApplication] browserAppDelegate].defaultPreferences;

    BOOL currentlyEnabled = [preferences _isEnabledForInternalDebugFeature:feature];
    [preferences _setEnabled:!currentlyEnabled forInternalDebugFeature:feature];

    [[NSUserDefaults standardUserDefaults] setBool:!currentlyEnabled forKey:feature.key];
}

- (void)resetAllExperimentalFeatures:(id)sender
{
    WKPreferences *preferences = [[NSApplication sharedApplication] browserAppDelegate].defaultPreferences;
    NSArray<_WKExperimentalFeature *> *experimentalFeatures = [WKPreferences _experimentalFeatures];

    for (_WKExperimentalFeature *feature in experimentalFeatures) {
        [preferences _setEnabled:feature.defaultValue forExperimentalFeature:feature];
        [[NSUserDefaults standardUserDefaults] setBool:feature.defaultValue forKey:feature.key];
    }
}

- (void)resetAllInternalDebugFeatures:(id)sender
{
    WKPreferences *preferences = [[NSApplication sharedApplication] browserAppDelegate].defaultPreferences;
    NSArray<_WKInternalDebugFeature *> *internalDebugFeatures = [WKPreferences _internalDebugFeatures];

    for (_WKInternalDebugFeature *feature in internalDebugFeatures) {
        [preferences _setEnabled:feature.defaultValue forInternalDebugFeature:feature];
        [[NSUserDefaults standardUserDefaults] setBool:feature.defaultValue forKey:feature.key];
    }
}

- (BOOL)debugOverlayVisible:(NSMenuItem *)menuItem
{
    NSString *preferenceKey = [self preferenceKeyForRegionOverlayTag:[menuItem tag]];
    if (preferenceKey)
        return [[NSUserDefaults standardUserDefaults] boolForKey:preferenceKey];

    return NO;
}

- (void)toggleSiteIsolationOverlay:(id)sender
{
    [self _toggleBooleanDefault:SiteIsolationOverlayPreferenceKey];
}

- (BOOL)siteIsolationOverlayEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:SiteIsolationOverlayPreferenceKey];
}

- (BOOL)siteIsolationOverlayVisible:(NSMenuItem *)menuItem
{
    return [self siteIsolationOverlayEnabled];
}

- (NSString *)customUserAgent
{
    NSString *uaIdentifier = [[NSUserDefaults standardUserDefaults] stringForKey:CustomUserAgentPreferenceKey];
    if (uaIdentifier) {
        for (NSDictionary *item in [[self class] userAgentData]) {
            if ([item[@"identifier"] isEqualToString:uaIdentifier])
                return item[@"userAgent"];
        }
    }

    return nil;
}

- (void)changeCustomUserAgent:(id)sender
{
    NSDictionary *userAgentDict = [sender representedObject];
    if (!userAgentDict)
        return;

    NSString *uaIdentifier = userAgentDict[@"identifier"];
    if (uaIdentifier)
        [[NSUserDefaults standardUserDefaults] setObject:uaIdentifier forKey:CustomUserAgentPreferenceKey];

    [[NSNotificationCenter defaultCenter] postNotificationName:kUserAgentChangedNotificationName object:self];
}

- (AttachmentElementEnabledState)attachmentElementEnabled
{
    return (AttachmentElementEnabledState)[[NSUserDefaults standardUserDefaults] integerForKey:AttachmentElementEnabledPreferenceKey];
}

- (NSInteger)preferenceValueForAttachmentElementEnabledTag:(NSUInteger)tag
{
    switch (tag) {
    case AttachmentElementDisabledTag:
        return AttachmentElementEnabledStateDisabled;

    case AttachmentElementEnabledTag:
        return AttachmentElementEnabledStateEnabled;

    case AttachmentElementWideLayoutEnabledTag:
        return AttachmentElementEnabledStateWideLayoutEnabled;
    }
    return AttachmentElementEnabledStateDisabled;
}

- (BOOL)attachmentElementEnabled:(NSMenuItem *)menuItem
{
    NSInteger value = [[NSUserDefaults standardUserDefaults] integerForKey:AttachmentElementEnabledPreferenceKey];
    return [self preferenceValueForAttachmentElementEnabledTag:[menuItem tag]] == value ? YES : NO;
}

- (void)changeAttachmentElementEnabled:(id)sender
{
    NSInteger value = [self preferenceValueForAttachmentElementEnabledTag:[sender tag]];
    [[NSUserDefaults standardUserDefaults] setInteger:value forKey:AttachmentElementEnabledPreferenceKey];
    [[[NSApplication sharedApplication] browserAppDelegate] didChangeSettings];
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
