/*	
    WebLocalizableStrings.m
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebLocalizableStrings.h>

#import <WebKit/WebAssertions.h>

WebLocalizableStringsBundle WebKitLocalizableStringsBundle = { "com.apple.WebKit", 0 };

NSString *WebLocalizedString(WebLocalizableStringsBundle *stringsBundle, const char *key)
{
    NSBundle *bundle;
    if (stringsBundle == NULL) {
        static NSBundle *mainBundle;
        if (mainBundle == nil) {
            mainBundle = [NSBundle mainBundle];
            ASSERT(mainBundle);
        }
        bundle = mainBundle;
    } else {
        bundle = stringsBundle->bundle;
        if (bundle == nil) {
            bundle = [NSBundle bundleWithIdentifier:[NSString stringWithUTF8String:stringsBundle->identifier]];
            ASSERT(bundle);
            stringsBundle->bundle = bundle;
        }
    }
    NSString *notFound = @"localized string not found";
    CFStringRef keyString = CFStringCreateWithCStringNoCopy(NULL, key, kCFStringEncodingUTF8, kCFAllocatorNull);
    NSString *result = [bundle localizedStringForKey:(NSString *)keyString value:notFound table:nil];
    CFRelease(keyString);
    ASSERT_WITH_MESSAGE(result != notFound, "could not find localizable string %s in bundle", key);
    return result;
}
