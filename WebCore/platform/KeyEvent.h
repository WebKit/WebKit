/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
 */

#ifndef KeyEvent_H
#define KeyEvent_H

#include "PlatformString.h"

#ifdef __OBJC__
@class NSEvent;
#else
class NSEvent;
#endif

#if WIN32
typedef struct HWND__ *HWND;
typedef unsigned    WPARAM;
typedef long        LPARAM;
#endif

namespace WebCore {

    class KeyEvent {
    public:
        String text() const { return m_text; }
        String unmodifiedText() const { return m_unmodifiedText; }
        String keyIdentifier() const { return m_keyIdentifier; }
        bool isKeyUp() const { return m_isKeyUp; }
        bool isAutoRepeat() const { return m_autoRepeat; }
        int WindowsKeyCode() const { return m_WindowsKeyCode; }
        bool isKeypad() const { return m_isKeypad; }
        bool shiftKey() const { return m_shiftKey; }
        bool ctrlKey() const { return m_ctrlKey; }
        bool altKey() const { return m_altKey; }
        bool metaKey() const { return m_metaKey; }

#ifdef __APPLE__
        KeyEvent(NSEvent*, bool forceAutoRepeat = false);
#endif

#ifdef WIN32
        KeyEvent(HWND, WPARAM, LPARAM);
#endif

    private:
        String m_text;
        String m_unmodifiedText;
        String m_keyIdentifier;
        bool m_isKeyUp;
        bool m_autoRepeat;
        int m_WindowsKeyCode;
        bool m_isKeypad;
        bool m_shiftKey;
        bool m_ctrlKey;
        bool m_altKey;
        bool m_metaKey;
    };

}

#endif
