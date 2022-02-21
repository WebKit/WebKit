/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "WKHoverPlatterParameters.h"

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)

#import "PrototypeToolsSPI.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/RetainPtr.h>

@interface PTSliderRow (WebKit)

- (id)integerMinValue:(CGFloat)minValue maxValue:(CGFloat)maxValue;

@end

@implementation PTSliderRow (WebKit)

- (id)integerMinValue:(CGFloat)minValue maxValue:(CGFloat)maxValue
{
    return [[self minValue:minValue maxValue:maxValue] valueValidator:^id(id proposedValue, id settings) {
        return @(CGRound([proposedValue floatValue]));
    }];
}

@end

@implementation WKHoverPlatterParameters

- (void)setDefaultValues
{
    [super setDefaultValues];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    auto boolDefault = ^(NSString *name) {
        return [defaults boolForKey:name];
    };

    _platterEnabledForMouse = boolDefault(@"WKHoverPlatterEnabledForMouse");
    _platterEnabledForLongPress = boolDefault(@"WKHoverPlatterEnabledForLongPress");
    _platterEnabledForSingleTap = boolDefault(@"WKHoverPlatterEnabledForSingleTap");
    _platterEnabledForDoubleTap = boolDefault(@"WKHoverPlatterEnabledForDoubleTap");

    _showDebugOverlay = boolDefault(@"WKHoverPlatterShowDebugOverlay");

    _platterRadius = 200;
    _platterScale = 2;

    _linkSearchRadius = 40;

    _springMass = 2;
    _springStiffness = 200;
    _springDamping = 200;
    _duration = 0.3;
    _useSpring = YES;
    _animateScale = YES;
}

- (BOOL)enabled
{
    return _platterEnabledForMouse
        || _platterEnabledForLongPress
        || _platterEnabledForSingleTap
        || _platterEnabledForDoubleTap;
}

+ (PTModule *)settingsControllerModule
{
    PTSection *enablementSection = [PTModule sectionWithRows:@[
        [PTSwitchRow rowWithTitle:@"Enable Platter For Mouse" valueKeyPath:@"platterEnabledForMouse"],
        [PTSwitchRow rowWithTitle:@"Enable Platter For Long Press" valueKeyPath:@"platterEnabledForLongPress"],
        [PTSwitchRow rowWithTitle:@"Enable Platter For Single Tap" valueKeyPath:@"platterEnabledForSingleTap"],
        [PTSwitchRow rowWithTitle:@"Enable Platter For Double Tap" valueKeyPath:@"platterEnabledForDoubleTap"],
    ] title:@"Platter"];

    PTSection *debugSection = [PTModule sectionWithRows:@[
        [PTSwitchRow rowWithTitle:@"Show Debug Overlay" valueKeyPath:@"showDebugOverlay"],
    ] title:@"Debug"];

    PTSection *animationSection = [PTModule sectionWithRows:@[
        [PTSwitchRow rowWithTitle:@"Animate Scale" valueKeyPath:@"animateScale"],
        [PTSwitchRow rowWithTitle:@"Use Spring" valueKeyPath:@"useSpring"],
        [[[PTSliderRow rowWithTitle:@"Duration" valueKeyPath:@"duration"] minValue:0 maxValue:1] condition:[NSPredicate predicateWithFormat:@"useSpring == FALSE"]],
        [[PTEditFloatRow rowWithTitle:@"Spring Mass" valueKeyPath:@"springMass"] condition:[NSPredicate predicateWithFormat:@"useSpring == TRUE"]],
        [[PTEditFloatRow rowWithTitle:@"Spring Stiffness" valueKeyPath:@"springStiffness"] condition:[NSPredicate predicateWithFormat:@"useSpring == TRUE"]],
        [[PTEditFloatRow rowWithTitle:@"Spring Damping" valueKeyPath:@"springDamping"] condition:[NSPredicate predicateWithFormat:@"useSpring == TRUE"]],
    ] title:@"Animation"];

    PTSection *platterSection = [PTModule sectionWithRows:@[
        [[PTSliderRow rowWithTitle:@"Radius" valueKeyPath:@"platterRadius"] integerMinValue:0 maxValue:400],
        [[PTSliderRow rowWithTitle:@"Scale" valueKeyPath:@"platterScale"] minValue:1 maxValue:4],
    ] title:@"Platter"];

    PTSection *linkSearchingSection = [PTModule sectionWithRows:@[
        [[PTSliderRow rowWithTitle:@"Radius" valueKeyPath:@"linkSearchRadius"] integerMinValue:0 maxValue:200],
    ] title:@"Link Searching"];

    PTSection *restoreDefaultsSection = [PTModule sectionWithRows:@[
        [PTButtonRow rowWithTitle:@"Restore Defaults" action:[PTRestoreDefaultSettingsRowAction action]]
    ]];

    return [PTModule moduleWithTitle:nil contents:@[ enablementSection, debugSection, animationSection, platterSection, linkSearchingSection, restoreDefaultsSection ]];
}

@end

@implementation WKHoverPlatterDomain

+ (WKHoverPlatterParameters *)rootSettings
{
    return [super rootSettings];
}

+ (NSString *)domainGroupName
{
    return @"WebKit";
}

+ (NSString *)domainName
{
    return @"Hover Platter";
}

+ (Class)rootSettingsClass
{
    return [WKHoverPlatterParameters class];
}

@end

#endif // HAVE(UIKIT_WITH_MOUSE_SUPPORT)
