/*
    WebNSUserDefaultsExtras.m
    Private (SPI) header
    Copyright 2005, Apple, Inc. All rights reserved.
 */

#import <WebKit/WebNSUserDefaultsExtras.h>

#import <WebKit/WebAssertions.h>
#import <WebKitSystemInterface.h>

@implementation NSString (WebNSUserDefaultsPrivate)

- (NSString *)_webkit_HTTPStyleLanguageCode
{
    // Look up the language code using CFBundle.
    NSString *languageCode = self;
    NSString *preferredLanguageCode = [(id)WKCopyCFLocalizationPreferredName((CFStringRef)self) autorelease];

    if (preferredLanguageCode)
        languageCode = preferredLanguageCode;
    
    // Make the string lowercase.
    NSString *lowercaseLanguageCode = [languageCode lowercaseString];
    
    // Turn a '_' into a '-' if it appears after a 2-letter language code.
    if ([lowercaseLanguageCode length] < 3 || [lowercaseLanguageCode characterAtIndex:2] != '_') {
        return lowercaseLanguageCode;
    }
    NSMutableString *result = [lowercaseLanguageCode mutableCopy];
    [result replaceCharactersInRange:NSMakeRange(2, 1) withString:@"-"];
    return [result autorelease];
}

@end

@implementation NSUserDefaults (WebNSUserDefaultsExtras)

static NSString *preferredLanguageCode = nil;
static NSLock *preferredLanguageLock = nil;
static pthread_once_t preferredLanguageLockOnce = PTHREAD_ONCE_INIT;
static pthread_once_t addDefaultsChangeObserverOnce = PTHREAD_ONCE_INIT;

static void makeLock(void)
{
    preferredLanguageLock = [[NSLock alloc] init]; 
}

+ (void)_ensureAndLockPreferredLanguageLock
{
    pthread_once(&preferredLanguageLockOnce, makeLock);
    [preferredLanguageLock lock];
}

+ (void)_webkit_defaultsDidChange
{
    [self _ensureAndLockPreferredLanguageLock];

    [preferredLanguageCode release];
    preferredLanguageCode = nil;

    [preferredLanguageLock unlock];
}

static void addDefaultsChangeObserver(void)
{
    [[NSNotificationCenter defaultCenter] addObserver:[NSUserDefaults class]
                                             selector:@selector(_webkit_defaultsDidChange)
                                                 name:NSUserDefaultsDidChangeNotification
                                               object:[NSUserDefaults standardUserDefaults]];
}

+ (void)_webkit_addDefaultsChangeObserver
{
    pthread_once(&addDefaultsChangeObserverOnce, addDefaultsChangeObserver);
}

+ (NSString *)_webkit_preferredLanguageCode
{
    // Get this outside the lock since it might block on the defaults lock, while we are inside registerDefaults:.
    NSUserDefaults *standardDefaults = [self standardUserDefaults];

    BOOL addObserver = NO;

    [self _ensureAndLockPreferredLanguageLock];

    if (!preferredLanguageCode) {
        NSArray *languages = [standardDefaults stringArrayForKey:@"AppleLanguages"];
        if ([languages count] == 0) {
            preferredLanguageCode = [@"en" retain];
        } else {
            preferredLanguageCode = [[[languages objectAtIndex:0] _webkit_HTTPStyleLanguageCode] copy];
        }
        addObserver = YES;
    }

    NSString *code = [[preferredLanguageCode retain] autorelease];
    
    [preferredLanguageLock unlock];

    if (addObserver) {
        [self _webkit_addDefaultsChangeObserver];
    }

    return code;
}

@end
