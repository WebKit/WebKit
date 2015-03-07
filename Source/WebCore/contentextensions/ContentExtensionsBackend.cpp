/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ContentExtensionsBackend.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "CompiledContentExtension.h"
#include "DFABytecodeInterpreter.h"
#include "URL.h"
#include <wtf/text/CString.h>

namespace WebCore {

namespace ContentExtensions {
    
void ContentExtensionsBackend::addContentExtension(const String& identifier, RefPtr<CompiledContentExtension> compiledContentExtension)
{
    ASSERT(!identifier.isEmpty());
    if (identifier.isEmpty())
        return;

    if (!compiledContentExtension) {
        removeContentExtension(identifier);
        return;
    }

    m_contentExtensions.set(identifier, compiledContentExtension);
}

void ContentExtensionsBackend::removeContentExtension(const String& identifier)
{
    m_contentExtensions.remove(identifier);
}

void ContentExtensionsBackend::removeAllContentExtensions()
{
    m_contentExtensions.clear();
}

Vector<Action> ContentExtensionsBackend::actionsForURL(const URL& url)
{
    const String& urlString = url.string();
    ASSERT_WITH_MESSAGE(urlString.containsOnlyASCII(), "A decoded URL should only contain ASCII characters. The matching algorithm assumes the input is ASCII.");
    const CString& urlCString = urlString.utf8();

    Vector<Action> finalActions;
    for (auto& compiledContentExtension : m_contentExtensions.values()) {
        DFABytecodeInterpreter interpreter(compiledContentExtension->bytecode(), compiledContentExtension->bytecodeLength());
        DFABytecodeInterpreter::Actions triggeredActions = interpreter.interpret(urlCString);
        
        const SerializedActionByte* actions = compiledContentExtension->actions();
        const unsigned actionsLength = compiledContentExtension->actionsLength();
        
        if (!triggeredActions.isEmpty()) {
            Vector<unsigned> actionLocations;
            actionLocations.reserveInitialCapacity(triggeredActions.size());
            for (auto actionLocation : triggeredActions)
                actionLocations.append(static_cast<unsigned>(actionLocation));
            std::sort(actionLocations.begin(), actionLocations.end());
            
            // Add actions in reverse order to properly deal with IgnorePreviousRules.
            for (unsigned i = actionLocations.size(); i; i--) {
                Action action = Action::deserialize(actions, actionsLength, actionLocations[i - 1]);
                if (action.type() == ActionType::IgnorePreviousRules)
                    break;
                finalActions.append(action);
            }
        }
    }
    return finalActions;
}

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
