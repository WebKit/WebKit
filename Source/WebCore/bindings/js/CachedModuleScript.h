/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "CachedModuleScriptLoader.h"
#include "LoadableScript.h"
#include <runtime/JSInternalPromise.h>

namespace WebCore {

class CachedModuleScriptClient;
class Document;
class ScriptSourceCode;

class CachedModuleScript : public RefCounted<CachedModuleScript> {
public:
    UniquedStringImpl* moduleKey() { return m_moduleKey.get(); }

    void notifyLoadCompleted(UniquedStringImpl& moduleKey);
    void notifyLoadFailed(LoadableScript::Error&&);
    void notifyLoadWasCanceled();

    const std::optional<LoadableScript::Error>& error() const { return m_error; }
    bool wasCanceled() const { return m_wasCanceled; }
    bool isLoaded() const { return m_isLoaded; }

    void addClient(CachedModuleScriptClient&);
    void removeClient(CachedModuleScriptClient&);

    static Ref<CachedModuleScript> create();

    void load(Document&, const URL& rootURL, LoadableScript&);
    void load(Document&, const ScriptSourceCode&, LoadableScript&);

private:
    void notifyClientFinished();

    RefPtr<UniquedStringImpl> m_moduleKey;
    HashCountedSet<CachedModuleScriptClient*> m_clients;
    std::optional<LoadableScript::Error> m_error;
    bool m_wasCanceled { false };
    bool m_isLoaded { false };
};

} // namespace WebCore
