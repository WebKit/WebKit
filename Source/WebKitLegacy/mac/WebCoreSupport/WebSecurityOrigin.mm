/*
 * Copyright (C) 2007, 2010, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebSecurityOriginInternal.h"

#import "WebDatabaseQuotaManager.h"
#import "WebQuotaManager.h"
#import <WebCore/DatabaseTracker.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/SecurityOriginData.h>
#import <wtf/URL.h>


@implementation WebSecurityOrigin {
    RefPtr<WebCore::SecurityOrigin> _origin;
    RetainPtr<WebDatabaseQuotaManager> _databaseQuotaManager;
}

+ (id)webSecurityOriginFromDatabaseIdentifier:(NSString *)databaseIdentifier
{
    WTF::initializeMainThread();

    auto origin = WebCore::SecurityOriginData::fromDatabaseIdentifier(String { databaseIdentifier });
    if (!origin)
        return nil;

    return adoptNS([[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:origin->securityOrigin().ptr()]).autorelease();
}

- (id)initWithURL:(NSURL *)url
{
    WTF::initializeMainThread();

    self = [super init];
    if (!self)
        return nil;

    _origin = WebCore::SecurityOrigin::create(URL([url absoluteURL]));
    return self;
}

- (NSString *)protocol
{
    return protect(_origin)->protocol().createNSString().autorelease();
}

- (NSString *)host
{
    return protect(_origin)->host().createNSString().autorelease();
}

- (NSString *)databaseIdentifier
{
    return _origin->data().databaseIdentifier().createNSString().autorelease();
}

#if PLATFORM(IOS_FAMILY)
- (NSString *)toString
{
    return protect(_origin)->toString().createNSString().autorelease();
}
#endif

- (NSString *)stringValue
{
    return protect(_origin)->toString().createNSString().autorelease();
}

- (unsigned short)port
{
    return protect(_origin)->port().value_or(0);
}

// FIXME: Overriding isEqual: without overriding hash will cause trouble if this ever goes into an NSSet or is the key in an NSDictionary,
// since two equal objects could have different hashes.
- (BOOL)isEqual:(id)anObject
{
    if (![anObject isMemberOfClass:[WebSecurityOrigin class]])
        return NO;
    
    return [self _core]->equal(*[anObject _core]);
}

@end

@implementation WebSecurityOrigin (WebInternal)

- (id)_initWithWebCoreSecurityOrigin:(WebCore::SecurityOrigin*)origin
{
    ASSERT(origin);
    self = [super init];
    if (!self)
        return nil;

    _origin = origin;

    return self;
}

- (id)_initWithString:(NSString *)originString
{
    auto origin = WebCore::SecurityOrigin::createFromString(originString);
    return adoptNS([[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:origin.ptr()]).autorelease();
}

- (WebCore::SecurityOrigin *)_core
{
    return _origin.get();
}

@end


// MARK: -
// MARK: WebQuotaManagers

@implementation WebSecurityOrigin (WebQuotaManagers)

- (id<WebQuotaManager>)databaseQuotaManager
{
    if (!_databaseQuotaManager)
        _databaseQuotaManager = adoptNS([[WebDatabaseQuotaManager alloc] initWithOrigin:self]);
    return _databaseQuotaManager;
}

@end


// MARK: -
// MARK: Deprecated

// FIXME: The following methods are deprecated and should removed later.
// Clients should instead get a WebQuotaManager, and query / set the quota via the Manager.

@implementation WebSecurityOrigin (Deprecated)

- (unsigned long long)usage
{
    return WebCore::DatabaseTracker::singleton().usage(_origin->data());
}

- (unsigned long long)quota
{
    return WebCore::DatabaseTracker::singleton().quota(_origin->data());
}

- (void)setQuota:(unsigned long long)quota
{
    WebCore::DatabaseTracker::singleton().setQuota(_origin->data(), quota);
}

@end
