/*
 * Copyright (C) 2012, 2020 Apple Inc. All rights reserved.
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

#import "config.h"
#import "AlternativeTextContextController.h"

#import <wtf/RetainPtr.h>

#if USE(APPKit)
#import <AppKit/NSTextAlternatives.h>
#elif PLATFORM(IOS_FAMILY)
#import <pal/spi/ios/UIKitSPI.h>
#endif

namespace WebCore {

AlternativeTextContextController::AlternativeTextContextController() = default;

AlternativeTextContextController::~AlternativeTextContextController() = default;

uint64_t AlternativeTextContextController::addAlternatives(NSTextAlternatives *alternatives)
{
    // FIXME: Turning a pointer into an integer is a flawed algorithm to generate a unique ID. Can lead to aliasing to a new object that happens to occupy the same memory as an old one.
    uint64_t context = reinterpret_cast<uint64_t>(alternatives);
    if (!context)
        return invalidContext;
    if (alternativesForContext(context))
        return context;
    auto result = m_alternativesObjectMap.add(context, alternatives);
    return result.isNewEntry ? context : invalidContext;
}

NSTextAlternatives *AlternativeTextContextController::alternativesForContext(uint64_t context)
{
    return m_alternativesObjectMap.get(context).get();
}

void AlternativeTextContextController::removeAlternativesForContext(uint64_t context)
{
    m_alternativesObjectMap.remove(context);
}

void AlternativeTextContextController::clear()
{
    m_alternativesObjectMap.clear();
}

} // namespace WebCore
