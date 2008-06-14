/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#ifndef TextEvent_h
#define TextEvent_h

#include "UIEvent.h"

namespace WebCore {

    class TextEvent : public UIEvent {
    public:
        static PassRefPtr<TextEvent> create()
        {
            return adoptRef(new TextEvent);
        }
        static PassRefPtr<TextEvent> create(PassRefPtr<AbstractView> view, const String& data)
        {
            return adoptRef(new TextEvent(view, data));
        }
        virtual ~TextEvent();
    
        void initTextEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<AbstractView>, const String& data);
    
        String data() const { return m_data; }

        virtual bool isTextEvent() const;

        // If true, any newline characters in the text are line breaks only, not paragraph separators.
        bool isLineBreak() const { return m_isLineBreak; }
        void setIsLineBreak(bool isLineBreak) { m_isLineBreak = isLineBreak; }

        // If true, any tab characters in the text are backtabs.
        bool isBackTab() const { return m_isBackTab; }
        void setIsBackTab(bool isBackTab) { m_isBackTab = isBackTab; }

    private:
        TextEvent();
        TextEvent(PassRefPtr<AbstractView>, const String& data);

        String m_data;
        bool m_isLineBreak;
        bool m_isBackTab;
    };

} // namespace WebCore

#endif // TextEvent_h
