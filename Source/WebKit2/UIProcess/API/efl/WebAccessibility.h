/*
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#ifndef WebAccessibility_h
#define WebAccessibility_h

#if HAVE(ACCESSIBILITY) && defined(HAVE_ECORE_X)

#include <Ecore.h>
#include <Ecore_X.h>
#include <Eina.h>
#include <WebCore/IntPoint.h>

class EwkView;

namespace WebKit {

class WebAccessibility {
public:
    explicit WebAccessibility(EwkView* ewkView);
    ~WebAccessibility();

    bool activateAction() const { return m_activateAction; }
    bool nextAction() const { return m_nextAction; }
    bool prevAction() const { return m_prevAction; }
    bool readAction() const { return m_readAction; }

private:
    static Eina_Bool eventHandler(void*, int, void*);

    Eina_Bool action(Ecore_X_Event_Client_Message*);

    // Actions
    bool activate();
    bool read(int, int);
    bool readNext();
    bool readPrev();
    bool up();
    bool down();
    bool scroll();
    bool zoom();
    bool mouse();
    bool enable();
    bool disable();

    bool m_activateAction;
    bool m_nextAction;
    bool m_prevAction;
    bool m_readAction;

    WebCore::IntPoint m_currentPoint;
    EwkView* m_ewkView;
    Ecore_Event_Handler* m_eventHandler;
};

} // namespace WebKit

#endif // HAVE(ACCESSIBILITY) && defined(HAVE_ECORE_X)

#endif // WebAccessibility_h
