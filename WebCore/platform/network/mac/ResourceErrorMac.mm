/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "ResourceError.h"

#import "BlockExceptions.h"
#import "KURL.h"
#import <Foundation/Foundation.h>

@interface NSError (WebExtras)
- (NSString *)_web_localizedDescription;
@end

namespace WebCore {

void ResourceError::platformLazyInit()
{
    if (m_dataIsUpToDate)
        return;

    m_domain = [m_platformError.get() domain];
    m_errorCode = [m_platformError.get() code];

    NSString* failingURLString = [[m_platformError.get() userInfo] valueForKey:@"NSErrorFailingURLStringKey"];
    if (!failingURLString)
        failingURLString = [[[m_platformError.get() userInfo] valueForKey:@"NSErrorFailingURLKey"] absoluteString];
    
    // Workaround for <rdar://problem/6554067>
    m_localizedDescription = failingURLString;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    m_localizedDescription = [m_platformError.get() _web_localizedDescription];
    END_BLOCK_OBJC_EXCEPTIONS;

    m_dataIsUpToDate = true;
}

bool ResourceError::platformCompare(const ResourceError& a, const ResourceError& b)
{
    return (NSError*)a == (NSError*)b;
}

ResourceError::operator NSError*() const
{
    if (m_isNull) {
        ASSERT(!m_platformError);
        return nil;
    }
    
    if (!m_platformError) {
        RetainPtr<NSMutableDictionary> userInfo(AdoptNS, [[NSMutableDictionary alloc] init]);

        if (!m_localizedDescription.isEmpty())
            [userInfo.get() setValue:m_localizedDescription forKey:NSLocalizedDescriptionKey];

        if (!m_failingURL.isEmpty()) {
            NSURL *cocoaURL = KURL(ParsedURLString, m_failingURL);
            [userInfo.get() setValue:m_failingURL forKey:@"NSErrorFailingURLStringKey"];
            [userInfo.get() setValue:cocoaURL forKey:@"NSErrorFailingURLKey"];
        }

        m_platformError.adoptNS([[NSError alloc] initWithDomain:m_domain code:m_errorCode userInfo:userInfo.get()]);
    }

    return m_platformError.get();
}

} // namespace WebCore
