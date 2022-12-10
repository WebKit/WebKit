/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)
#if USE(APPLE_INTERNAL_SDK)

// This space intentionally left blank

#else

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, LSInstallType) {
    LSInstallTypeIntentionalDowngrade = 8
};

@interface LSRecord : NSObject
@end

@interface LSBundleRecord : LSRecord
@property (readonly, nullable) NSString *bundleIdentifier;
@end

@interface LSApplicationRecord : LSBundleRecord
@property (readonly) BOOL placeholder;
@property (readonly) NSString *managementDomain;

- (instancetype)initWithBundleIdentifier:(NSString *)bundleIdentifier allowPlaceholder:(BOOL)allowPlaceholder error:(NSError **)outError;
@end

@interface LSEnumerator<__covariant ObjectType> : NSEnumerator<ObjectType>
@end

typedef NS_OPTIONS(uint64_t, LSApplicationEnumerationOptions) {
    LSApplicationEnumerationOptionsEnumeratePlaceholders = (1 << 6)
};

@interface LSApplicationRecord (Enumeration)
+ (LSEnumerator<LSApplicationRecord *> *)enumeratorWithOptions:(LSApplicationEnumerationOptions)options;
@end

@interface _LSOpenConfiguration : NSObject <NSCopying, NSSecureCoding>
@property (readwrite) BOOL sensitive;
@property (readwrite) BOOL allowURLOverrides;
@property (readwrite, copy, nullable) NSDictionary<NSString *, id> *frontBoardOptions;
@end

@interface LSApplicationWorkspace : NSObject
+ (LSApplicationWorkspace *)defaultWorkspace;
- (void)openURL:(NSURL *)url configuration:(_LSOpenConfiguration *)configuration completionHandler:(void (^)(NSDictionary<NSString *, id> *result, NSError *error))completionHandler;
@end

NS_ASSUME_NONNULL_END

#endif // USE(APPLE_INTERNAL_SDK)
#endif // PLATFORM(IOS)
