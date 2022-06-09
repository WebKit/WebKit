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

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT) || ENABLE(HOVER_GESTURE_RECOGNIZER)

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

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WKHoverPlatterParametersAdditions.mm>
#else
static void addAdditionalPlatterLayoutParameters(NSMutableArray *) { }
static void setDefaultValuesForAdditionalPlatterLayoutParameters(WKHoverPlatterParameters *) { }
#endif

@implementation WKHoverPlatterParameters

- (void)setDefaultValues
{
    [super setDefaultValues];

    _platterEnabledForMouse = NO;
    _platterEnabledForHover = NO;

    _platterCornerRadius = 6;
    _platterPadding = 5;
    _platterShadowOpacity = 0.6;
    _platterShadowRadius = 10;
    _platterInflationSize = 8;

    _animateBetweenPlatters = YES;
    _springMass = 2;
    _springStiffness = 100;
    _springDamping = 50;
    _duration = 0.3;
    _useSpring = YES;

    setDefaultValuesForAdditionalPlatterLayoutParameters(self);
}

+ (PTModule *)settingsControllerModule
{
    PTSection *enablementSection = [PTModule sectionWithRows:@[
        [PTSwitchRow rowWithTitle:@"Enable Platter For Mouse" valueKeyPath:@"platterEnabledForMouse"],
        [PTSwitchRow rowWithTitle:@"Enable Platter For Hover" valueKeyPath:@"platterEnabledForHover"],
    ] title:@"Platter"];

    PTSection *animationSection = [PTModule sectionWithRows:@[
        [PTSwitchRow rowWithTitle:@"Animate Between Platters" valueKeyPath:@"animateBetweenPlatters"],
        [PTSwitchRow rowWithTitle:@"Use Spring" valueKeyPath:@"useSpring"],
        [[[PTSliderRow rowWithTitle:@"Duration" valueKeyPath:@"duration"] minValue:0 maxValue:1] condition:[NSPredicate predicateWithFormat:@"useSpring == FALSE"]],
        [[PTEditFloatRow rowWithTitle:@"Spring Mass" valueKeyPath:@"springMass"] condition:[NSPredicate predicateWithFormat:@"useSpring == TRUE"]],
        [[PTEditFloatRow rowWithTitle:@"Spring Stiffness" valueKeyPath:@"springStiffness"] condition:[NSPredicate predicateWithFormat:@"useSpring == TRUE"]],
        [[PTEditFloatRow rowWithTitle:@"Spring Damping" valueKeyPath:@"springDamping"] condition:[NSPredicate predicateWithFormat:@"useSpring == TRUE"]],
    ] title:@"Animation"];

    RetainPtr<NSMutableArray> platterLayoutRows = adoptNS([@[
        [[PTSliderRow rowWithTitle:@"Corner Radius" valueKeyPath:@"platterCornerRadius"] integerMinValue:0 maxValue:20],
        [[PTSliderRow rowWithTitle:@"Padding" valueKeyPath:@"platterPadding"] integerMinValue:0 maxValue:20],
        [[PTSliderRow rowWithTitle:@"Shadow Opacity" valueKeyPath:@"platterShadowOpacity"] minValue:0 maxValue:1],
        [[PTSliderRow rowWithTitle:@"Shadow Radius" valueKeyPath:@"platterShadowRadius"] integerMinValue:0 maxValue:20],
        [[PTSliderRow rowWithTitle:@"Inflation Size" valueKeyPath:@"platterInflationSize"] integerMinValue:0 maxValue:30]
    ] mutableCopy]);
    addAdditionalPlatterLayoutParameters(platterLayoutRows.get());

    PTSection *platterLayoutSection = [PTModule sectionWithRows:platterLayoutRows.get() title:@"Platter Layout"];

    PTSection *restoreDefaultsSection = [PTModule sectionWithRows:@[
        [PTButtonRow rowWithTitle:@"Restore Defaults" action:[PTRestoreDefaultSettingsRowAction action]]
    ]];

    return [PTModule moduleWithTitle:nil contents:@[ enablementSection, animationSection, platterLayoutSection, restoreDefaultsSection ]];
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

#endif // HAVE(UIKIT_WITH_MOUSE_SUPPORT) || ENABLE(HOVER_GESTURE_RECOGNIZER)
