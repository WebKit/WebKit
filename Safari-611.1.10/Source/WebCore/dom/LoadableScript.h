/*
 * Copyright (C) 2016 Apple, Inc. All Rights Reserved.
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

#include "ScriptElementCachedScriptFetcher.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/HashCountedSet.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class LoadableScriptClient;
class ScriptElement;

class LoadableScript : public ScriptElementCachedScriptFetcher {
public:
    enum class ErrorType {
        CachedScript,
        CrossOriginLoad,
        MIMEType,
        Nosniff,
        FailedIntegrityCheck,
    };

    struct ConsoleMessage {
        MessageSource source;
        MessageLevel level;
        String message;
    };

    struct Error {
        ErrorType type;
        Optional<ConsoleMessage> consoleMessage;
    };

    virtual ~LoadableScript() = default;

    virtual bool isLoaded() const = 0;
    virtual Optional<Error> error() const = 0;
    virtual bool wasCanceled() const = 0;

    virtual void execute(ScriptElement&) = 0;

    void addClient(LoadableScriptClient&);
    void removeClient(LoadableScriptClient&);

protected:
    LoadableScript(const String& nonce, ReferrerPolicy policy, const String& crossOriginMode, const String& charset, const AtomString& initiatorName, bool isInUserAgentShadowTree)
        : ScriptElementCachedScriptFetcher(nonce, policy, crossOriginMode, charset, initiatorName, isInUserAgentShadowTree)
    {
    }

    void notifyClientFinished();

private:
    HashCountedSet<LoadableScriptClient*> m_clients;
};

}
