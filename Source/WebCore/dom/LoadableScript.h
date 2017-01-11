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

#include "CachedResourceHandle.h"
#include <runtime/ConsoleTypes.h>
#include <runtime/JSScriptFetcher.h>
#include <wtf/HashCountedSet.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CachedScript;
class Document;
class LoadableScriptClient;
class ScriptElement;
class URL;

class LoadableScript : public JSC::ScriptFetcher {
public:
    enum class ErrorType {
        CachedScript,
        CrossOriginLoad,
        Nosniff,
    };

    struct ConsoleMessage {
        MessageSource source;
        MessageLevel level;
        String message;
    };

    struct Error {
        ErrorType type;
        std::optional<ConsoleMessage> consoleMessage;
    };

    virtual ~LoadableScript() { }

    virtual bool isLoaded() const = 0;
    virtual std::optional<Error> error() const = 0;
    virtual bool wasCanceled() const = 0;

    virtual void execute(ScriptElement&) = 0;

    void addClient(LoadableScriptClient&);
    void removeClient(LoadableScriptClient&);

    virtual bool isClassicScript() const { return false; }
    virtual bool isModuleScript() const { return false; }

    CachedResourceHandle<CachedScript> requestScriptWithCache(Document&, const URL& sourceURL) const;

protected:
    LoadableScript(const String& nonce, const String& crossOriginMode, const String& charset, const AtomicString& initiatorName, bool isInUserAgentShadowTree)
        : m_nonce(nonce)
        , m_crossOriginMode(crossOriginMode)
        , m_charset(charset)
        , m_initiatorName(initiatorName)
        , m_isInUserAgentShadowTree(isInUserAgentShadowTree)
    {
    }

    void notifyClientFinished();

private:
    String m_nonce;
    String m_crossOriginMode;
    String m_charset;
    AtomicString m_initiatorName;
    bool m_isInUserAgentShadowTree { false };

    HashCountedSet<LoadableScriptClient*> m_clients;
};

}
