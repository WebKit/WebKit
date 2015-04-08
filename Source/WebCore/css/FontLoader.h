/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#if ENABLE(FONT_LOAD_EVENTS)

#ifndef FontLoader_h
#define FontLoader_h

#include "ActiveDOMObject.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "Timer.h"
#include "VoidCallback.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class CachedFont;
class CSSFontFaceRule;
class CSSFontFaceSource;
class Dictionary;
class Document;
class Event;
class FontCascade;
class LoadFontCallback;
class ScriptExecutionContext;

class FontLoader final : public RefCounted<FontLoader>, public ActiveDOMObject, public EventTarget {
public:
    static Ref<FontLoader> create(Document* document)
    {
        return adoptRef<FontLoader>(*new FontLoader(document));
    }
    virtual ~FontLoader();

    bool checkFont(const String&, const String&);
    void loadFont(const Dictionary&);
    void loadFontDone(const LoadFontCallback&);

    void notifyWhenFontsReady(PassRefPtr<VoidCallback>);

    bool loading() const { return m_numLoadingFromCSS > 0 || m_numLoadingFromJS > 0; }

    virtual ScriptExecutionContext* scriptExecutionContext() const override;
    virtual EventTargetInterface eventTargetInterface() const override;

    using RefCounted<FontLoader>::ref;
    using RefCounted<FontLoader>::deref;

    Document* document() const { return m_document; }

    void didLayout();
    void beginFontLoading(CSSFontFaceRule*);
    void fontLoaded(CSSFontFaceRule*);
    void loadError(CSSFontFaceRule*, CSSFontFaceSource*);
    void loadingDone();

private:
    FontLoader(Document*);

    virtual void refEventTarget() override { ref(); }
    virtual void derefEventTarget() override { deref(); }
    virtual EventTargetData* eventTargetData() override;
    virtual EventTargetData& ensureEventTargetData() override;

    // ActiveDOMObject API.
    const char* activeDOMObjectName() const override;
    bool canSuspendForPageCache() const override;

    void pendingEventsTimerFired() { firePendingEvents(); }
    void scheduleEvent(PassRefPtr<Event>);
    void firePendingEvents();
    bool resolveFontStyle(const String&, FontCascade&);

    Document* m_document;
    EventTargetData m_eventTargetData;
    unsigned m_numLoadingFromCSS;
    unsigned m_numLoadingFromJS;
    Vector<RefPtr<Event>> m_pendingEvents;
    Vector<RefPtr<VoidCallback>> m_callbacks;
    RefPtr<Event> m_loadingDoneEvent;
    Timer m_pendingEventsTimer;
};

} // namespace WebCore

#endif // FontLoader_h
#endif // ENABLE(FONT_LOAD_EVENTS)
