//
//  WebLocalizedStringFactory.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Nov 20 2003.
//  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebLocalizedStringFactory.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebLocalizableStrings.h>

@implementation WebLocalizedStringFactory

+ (void)createSharedFactory
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];
    }
    ASSERT([[self sharedFactory] isKindOfClass:self]);
}

- (void)dealloc
{
    [keyGenerationMenuItemTitles release];
    [super dealloc];
}

- (NSArray *)keyGenerationMenuItemTitles
{
    if (!keyGenerationMenuItemTitles) {
        keyGenerationMenuItemTitles = [[NSArray alloc] initWithObjects:
            UI_STRING("2048 (High Grade)", "Menu item title for KEYGEN pop-up menu"),
            UI_STRING("1024 (Medium Grade)", "Menu item title for KEYGEN pop-up menu"),
            UI_STRING("512 (Low Grade)", "Menu item title for KEYGEN pop-up menu"), nil];
    }
    return keyGenerationMenuItemTitles;
}

@end
