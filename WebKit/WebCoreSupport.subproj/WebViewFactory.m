//
//  WebViewFactory.m
//  WebKit
//
//  Created by Darin Adler on Tue May 07 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebViewFactory.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebLocalizableStrings.h>

#import <WebKit/WebPluginDatabase.h>

@implementation WebViewFactory

+ (void)createSharedFactory;
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];
    }
    ASSERT([[self sharedFactory] isKindOfClass:self]);
}

- (NSArray *)pluginsInfo
{
    return [[WebPluginDatabase installedPlugins] plugins];
}

- (NSString *)inputElementAltText
{
    return UI_STRING_KEY("Submit", "Submit (input element)", "alt text for <input> elements with no alt, title, or value");
}

- (NSString *)resetButtonDefaultLabel
{
    return UI_STRING("Reset", "default label for Reset buttons in forms on web pages");
}

- (NSString *)searchableIndexIntroduction
{
    return UI_STRING("This is a searchable index. Enter search keywords: ",
        "text that appears at the start of nearly-obsolete web pages in the form of a 'searchable index'");
}

- (NSString *)submitButtonDefaultLabel
{
    return UI_STRING("Submit", "default label for Submit buttons in forms on web pages");
}

- (NSString *)defaultLanguageCode
{
    // FIXME: Need implementation. Defaults gives us a list of languages, but by name, not code.
    return @"en";
}

@end
