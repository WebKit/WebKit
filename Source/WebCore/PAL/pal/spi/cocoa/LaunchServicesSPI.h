/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>

#include <wtf/spi/darwin/XPCSPI.h>

#if USE(APPLE_INTERNAL_SDK)
#import <CoreServices/CoreServicesPriv.h>
#endif // USE(APPLE_INTERNAL_SDK)

#if HAVE(APP_LINKS)
@class LSAppLink;
typedef void (^LSAppLinkCompletionHandler)(LSAppLink *appLink, NSError *error);
typedef void (^LSAppLinkOpenCompletionHandler)(BOOL success, NSError *error);
#endif

#if !USE(APPLE_INTERNAL_SDK)

@interface LSResourceProxy : NSObject <NSCopying, NSSecureCoding>
@property (nonatomic, copy, readonly) NSString *localizedName;
@end

@interface LSBundleProxy : LSResourceProxy <NSSecureCoding>
@end

#if HAVE(APP_LINKS)
@interface LSApplicationProxy : LSBundleProxy <NSSecureCoding>
@end

@interface LSAppLink : NSObject <NSSecureCoding>
@end

@interface LSAppLink ()
#if HAVE(APP_LINKS_WITH_ISENABLED)
+ (NSArray<LSAppLink *> *)appLinksWithURL:(NSURL *)aURL limit:(NSUInteger)limit error:(NSError **)outError;
- (void)openWithCompletionHandler:(LSAppLinkOpenCompletionHandler)completionHandler;
@property (nonatomic, getter=isEnabled) BOOL enabled;
#else
+ (void)getAppLinkWithURL:(NSURL *)aURL completionHandler:(LSAppLinkCompletionHandler)completionHandler;
- (void)openInWebBrowser:(BOOL)inWebBrowser setAppropriateOpenStrategyAndWebBrowserState:(NSDictionary<NSString *, id> *)state completionHandler:(LSAppLinkOpenCompletionHandler)completionHandler;
#endif
+ (void)openWithURL:(NSURL *)aURL completionHandler:(LSAppLinkOpenCompletionHandler)completionHandler;
@property (readonly, strong) LSApplicationProxy *targetApplicationProxy;
@end
#endif

@interface NSURL ()
- (NSURL *)iTunesStoreURL;
@end

#if PLATFORM(MAC)
enum LSSessionID {
    kLSDefaultSessionID = -2,
};
#endif

#if HAVE(LSDATABASECONTEXT)
@interface LSDatabaseContext : NSObject
@property (class, readonly) LSDatabaseContext *sharedDatabaseContext;
@end
#endif

#endif // !USE(APPLE_INTERNAL_SDK)

#if HAVE(LSDATABASECONTEXT)
#if __has_include(<CoreServices/LSDatabaseContext+WebKit.h>)
#import <CoreServices/LSDatabaseContext+WebKit.h>
#else
@interface LSDatabaseContext (WebKitChangeTracking)
- (id <NSObject>)addDatabaseChangeObserver4WebKit:(void (^)(xpc_object_t change))observer;
- (void)removeDatabaseChangeObserver4WebKit:(id <NSObject>)token;
- (void)observeDatabaseChange4WebKit:(xpc_object_t)change;
@end
#endif
#endif

#if PLATFORM(MAC)

typedef const struct CF_BRIDGED_TYPE(id) __LSASN* LSASNRef;
typedef enum LSSessionID LSSessionID;
typedef struct ProcessSerialNumber ProcessSerialNumber;

WTF_EXTERN_C_BEGIN

extern const CFStringRef _kLSDisplayNameKey;

LSASNRef _LSGetCurrentApplicationASN();
OSStatus _LSSetApplicationInformationItem(LSSessionID, LSASNRef, CFStringRef keyToSetRef, CFTypeRef valueToSetRef, CFDictionaryRef* newInformationDictRef);
CFTypeRef _LSCopyApplicationInformationItem(LSSessionID, LSASNRef, CFTypeRef);

OSStatus _RegisterApplication(CFDictionaryRef, ProcessSerialNumber*);

WTF_EXTERN_C_END

#endif // PLATFORM(MAC)

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)

#if PLATFORM(MACCATALYST) && USE(APPLE_INTERNAL_SDK)
enum LSSessionID {
    kLSDefaultSessionID = -2,
};
#endif

WTF_EXTERN_C_BEGIN

typedef bool (^LSServerConnectionAllowedBlock) (CFDictionaryRef optionsRef);
void _LSSetApplicationLaunchServicesServerConnectionStatus(uint64_t flags, LSServerConnectionAllowedBlock block);
CFDictionaryRef _LSApplicationCheckIn(LSSessionID sessionID, CFDictionaryRef applicationInfo);

WTF_EXTERN_C_END

#endif // PLATFORM(MAC) || PLATFORM(MACCATALYST)
