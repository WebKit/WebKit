/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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

#if ENABLE(WEBXR)

#include "PlatformXR.h"
#include "ScriptWrappable.h"
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class Document;
class WebXRInputSource;
class XRInputSourceEvent;
class WebXRSession;

class WebXRInputSourceArray final : public ScriptWrappable {
    WTF_MAKE_ISO_ALLOCATED(WebXRInputSourceArray);
    friend UniqueRef<WebXRInputSourceArray> WTF::makeUniqueRefWithoutFastMallocCheck<WebXRInputSourceArray, WebCore::WebXRSession&>(WebCore::WebXRSession&);
public:
    using InputSourceList = Vector<PlatformXR::FrameData::InputSource>;
    static UniqueRef<WebXRInputSourceArray> create(WebXRSession&);
    ~WebXRInputSourceArray();

    void ref();
    void deref();

    unsigned length() const;
    bool isSupportedPropertyIndex(unsigned index) const { return index < length(); }
    WebXRInputSource* item(unsigned) const;

    void clear();
    void update(double timestamp, const InputSourceList&);

    // For GC reachablitiy.
    WebXRSession* session() const { return &m_session; }

private:
    WebXRInputSourceArray(WebXRSession&);

    void handleRemovedInputSources(const InputSourceList&, Vector<RefPtr<WebXRInputSource>>&, Vector<RefPtr<WebXRInputSource>>&, Vector<Ref<XRInputSourceEvent>>&);
    void handleAddedOrUpdatedInputSources(double timestamp, const InputSourceList&, Vector<RefPtr<WebXRInputSource>>&, Vector<RefPtr<WebXRInputSource>>&, Vector<RefPtr<WebXRInputSource>>&, Vector<Ref<XRInputSourceEvent>>&);

    WebXRSession& m_session;
    Vector<Ref<WebXRInputSource>> m_inputSources;
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
