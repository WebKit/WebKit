/*	
    WebLocalizableStrings.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <Foundation/Foundation.h>

typedef struct {
    const char *identifier;
    NSBundle *bundle;
} WebLocalizableStringsBundle;

NSString *WebLocalizedString(WebLocalizableStringsBundle *bundle, const char *key);

#ifdef FRAMEWORK_NAME

#define LOCALIZABLE_STRINGS_BUNDLE(F) LOCALIZABLE_STRINGS_BUNDLE_HELPER(F)
#define LOCALIZABLE_STRINGS_BUNDLE_HELPER(F) F ## LocalizableStringsBundle
extern WebLocalizableStringsBundle LOCALIZABLE_STRINGS_BUNDLE(FRAMEWORK_NAME);

#define UI_STRING(string, comment) WebLocalizedString(&LOCALIZABLE_STRINGS_BUNDLE(FRAMEWORK_NAME), string)
#define UI_STRING_KEY(string, key, comment) WebLocalizedString(&LOCALIZABLE_STRINGS_BUNDLE(FRAMEWORK_NAME), key)

#else

#define UI_STRING(string, comment) WebLocalizedString(0, string)
#define UI_STRING_KEY(string, key, comment) WebLocalizedString(0, key)

#endif
