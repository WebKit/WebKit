/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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

#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/text/TextPosition.h>

namespace WebCore {

class CachedScript;
class Element;
class PendingScriptClient;

// A container for an external script which may be loaded and executed.
//
// A CachedResourceHandle alone does not prevent the underlying CachedResource
// from purging its data buffer. This class holds a dummy client open for its
// lifetime in order to guarantee that the data buffer will not be purged.
class PendingScript final : public RefCounted<PendingScript>, public CachedResourceClient {
public:
    static Ref<PendingScript> create(Element&, CachedScript&);
    static Ref<PendingScript> create(Element&, TextPosition scriptStartPosition);

    virtual ~PendingScript();

    TextPosition startingPosition() const { return m_startingPosition; }
    void setStartingPosition(const TextPosition& position) { m_startingPosition = position; }

    bool watchingForLoad() const { return needsLoading() && m_client; }

    Element& element() { return m_element.get(); }
    const Element& element() const { return m_element.get(); }

    CachedScript* cachedScript() const;
    bool needsLoading() const { return cachedScript(); }

    bool isLoaded() const;

    void notifyFinished(CachedResource*) override;

    void setClient(PendingScriptClient*);
    void clearClient();

private:
    PendingScript(Element&, CachedScript&);
    PendingScript(Element&, TextPosition startingPosition);

    void notifyClientFinished();

    Ref<Element> m_element;
    TextPosition m_startingPosition; // Only used for inline script tags.
    CachedResourceHandle<CachedScript> m_cachedScript;
    PendingScriptClient* m_client { nullptr };
};

}
