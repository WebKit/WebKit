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

namespace WebCore {

DictationContext AlternativeTextContextController::addAlternatives(NSTextAlternatives *alternatives)
{
    if (!alternatives)
        return { };
    return m_contexts.ensure(alternatives, [&] {
        auto context = DictationContext::generate();
        m_alternatives.add(context, alternatives);
        return context;
    }).iterator->value;
}

NSTextAlternatives *AlternativeTextContextController::alternativesForContext(DictationContext context) const
{
    if (!context)
        return nil;
    return m_alternatives.get(context).get();
}

void AlternativeTextContextController::removeAlternativesForContext(DictationContext context)
{
    if (!context)
        return;
    if (auto alternatives = m_alternatives.take(context))
        m_contexts.remove(alternatives);
}

void AlternativeTextContextController::clear()
{
    m_alternatives.clear();
    m_contexts.clear();
}

} // namespace WebCore
