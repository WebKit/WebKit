/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NavigationAction_h
#define NavigationAction_h

#include "FrameLoaderTypes.h"
#include "KURL.h"

#if PLATFORM(MAC)
#include "RetainPtr.h"
#ifdef __OBJC__
@class NSEvent;
#else
class NSEvent;
#endif
#endif

namespace WebCore {

    class NavigationAction {
    public:
        NavigationAction();
        NavigationAction(const KURL&, NavigationType);
        NavigationAction(const KURL&, FrameLoadType, bool isFormSubmission);
#if PLATFORM(MAC)
        // FIXME: Change from NSEvent to DOM event.
        NavigationAction(const KURL&, NavigationType, NSEvent *);
        NavigationAction(const KURL&, FrameLoadType, bool isFormSubmission, NSEvent *);
#endif

        bool isEmpty() const { return m_URL.isEmpty(); }

#if PLATFORM(MAC)
        KURL URL() const { return m_URL; }
        NavigationType type() const { return m_type; }
        NSEvent *event() const { return m_event.get(); }
#endif

    private:
        KURL m_URL;
        NavigationType m_type;
#if PLATFORM(MAC)
        RetainPtr<NSEvent> m_event;
#endif
    };

}

#endif
