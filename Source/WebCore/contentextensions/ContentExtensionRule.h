/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#ifndef ContentExtensionRule_h
#define ContentExtensionRule_h

#if ENABLE(CONTENT_EXTENSIONS)

#include "ContentExtensionActions.h"
#include "ResourceLoadInfo.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace ContentExtensions {

// A ContentExtensionRule is the smallest unit in a ContentExtension.
//
// It is composed of a trigger and an action. The trigger defines on what kind of content this extension should apply.
// The action defines what to perform on that content.

struct Trigger {
    String urlFilter;
    bool urlFilterIsCaseSensitive { false };
    ResourceFlags flags { 0 };
    bool operator==(const Trigger& other) const
    {
        return urlFilter == other.urlFilter
            && urlFilterIsCaseSensitive == other.urlFilterIsCaseSensitive
            && flags == other.flags;
    }
};
    
struct Action {
    Action()
        : m_type(ActionType::InvalidAction)
    {
    }

    Action(ActionType type, const String& stringArgument)
        : m_type(type)
        , m_stringArgument(stringArgument)
    {
        ASSERT(type == ActionType::CSSDisplayNoneSelector || type == ActionType::CSSDisplayNoneStyleSheet);
    }

    Action(ActionType type)
        : m_type(type)
    {
        ASSERT(type != ActionType::CSSDisplayNoneSelector && type != ActionType::CSSDisplayNoneStyleSheet);
    }

    bool operator==(const Action& other) const
    {
        return m_type == other.m_type
            && m_stringArgument == other.m_stringArgument;
    }

    static Action deserialize(const SerializedActionByte* actions, const unsigned actionsLength, unsigned location);

    ActionType type() const { return m_type; }
    const String& stringArgument() const { return m_stringArgument; }
        
private:
    ActionType m_type;
    String m_stringArgument;
};
    
class ContentExtensionRule {
public:
    ContentExtensionRule(const Trigger&, const Action&);

    const Trigger& trigger() const { return m_trigger; }
    const Action& action() const { return m_action; }

private:
    Trigger m_trigger;
    Action m_action;
};

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)

#endif // ContentExtensionRule_h
