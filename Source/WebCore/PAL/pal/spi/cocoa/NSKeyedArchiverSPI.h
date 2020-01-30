/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#import <Availability.h>
#import <wtf/Assertions.h>
#import <wtf/RetainPtr.h>

#if USE(APPLE_INTERNAL_SDK)
#import <Foundation/NSKeyedArchiver_Private.h>
#else
#import <Foundation/NSCoder.h>
#import <Foundation/NSKeyedArchiver.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSKeyedArchiver (NSKeyedArchiverSecureCodingInitializers)
- (instancetype)initRequiringSecureCoding:(BOOL)requiresSecureCoding API_AVAILABLE(macos(10.13), ios(11.0));
+ (nullable NSData *)archivedDataWithRootObject:(id)object requiringSecureCoding:(BOOL)requiresSecureCoding error:(NSError **)error API_AVAILABLE(macos(10.13), ios(11.0));
@end

@interface NSKeyedUnarchiver (NSKeyedUnarchiverSecureCodingInitializer)
- (nullable instancetype)initForReadingFromData:(NSData *)data error:(NSError **)error API_AVAILABLE(macos(10.13), ios(11.0));
+ (nullable id)unarchivedObjectOfClass:(Class)cls fromData:(NSData *)data error:(NSError **)error API_AVAILABLE(macos(10.13), ios(11.0));
+ (nullable id)unarchivedObjectOfClasses:(NSSet<Class> *)classes fromData:(NSData *)data error:(NSError **)error API_AVAILABLE(macos(10.13), ios(11.0));
@end

NS_ASSUME_NONNULL_END

#endif /* USE(APPLE_INTERNAL_SDK) */

inline NSData *_Nullable securelyArchivedDataWithRootObject(id _Nonnull object)
{
    NSError *error;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:object requiringSecureCoding:YES error:&error];
    if (!data)
        LOG_ERROR("Unable to archive data: %@", error);
    return data;
}

inline NSData *_Nullable insecurelyArchivedDataWithRootObject(id _Nonnull object)
{
    NSError *error;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:object requiringSecureCoding:NO error:&error];
    if (!data)
        LOG_ERROR("Unable to archive data: %@", error);
    return data;
}

inline id _Nullable insecurelyUnarchiveObjectFromData(NSData * _Nonnull data)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [NSKeyedUnarchiver unarchiveObjectWithData:data];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

inline id _Nullable unarchivedObjectOfClassesFromData(NSSet<Class> * _Nonnull classes, NSData * _Nonnull data)
{
    NSError *error;
    id value = [NSKeyedUnarchiver unarchivedObjectOfClasses:classes fromData:data error:&error];
    if (!value)
        LOG_ERROR("Unable to unarchive data: %@", error);
    return value;
}

inline RetainPtr<NSKeyedArchiver> secureArchiver()
{
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    return adoptNS(archiver);
}

inline RetainPtr<NSKeyedUnarchiver> secureUnarchiverFromData(NSData *_Nonnull data)
{
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
    unarchiver.decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;
    return adoptNS(unarchiver);
}

inline id _Nullable decodeObjectOfClassForKeyFromCoder(Class _Nonnull cls, NSString * _Nonnull key, NSCoder * _Nonnull coder)
{
    return [coder decodeObjectOfClass:cls forKey:key];
}
