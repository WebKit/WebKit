/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef InbandTextTrackPrivateAVFIOS_h
#define InbandTextTrackPrivateAVFIOS_h

#if ENABLE(VIDEO) && PLATFORM(IOS)

#include "InbandTextTrackPrivateAVF.h"
#include <wtf/RetainPtr.h>

namespace WebCore {

class InbandTextTrackPrivateAVFIOS : public InbandTextTrackPrivateAVF {
public:
    static PassRefPtr<InbandTextTrackPrivateAVFIOS> create(AVFInbandTrackParent* parent, int internalID, const String& name, const String& language, const String& type)
    {
        return adoptRef(new InbandTextTrackPrivateAVFIOS(parent, internalID, name, language, type));
    }

    ~InbandTextTrackPrivateAVFIOS();

    virtual InbandTextTrackPrivate::Kind kind() const override;
    virtual AtomicString label() const override { return m_name; }
    virtual AtomicString language() const override { return m_language; }
    virtual Category textTrackCategory() const override { return InBand; }
    int internalID() const { return m_internalID; }

protected:
    InbandTextTrackPrivateAVFIOS(AVFInbandTrackParent*, int, const String& name, const String& language, const String& type);

    String m_name;
    String m_language;
    String m_type;
    int m_internalID;
};
}

#endif // ENABLE(VIDEO) && PLATFORM(IOS)
#endif // InbandTextTrackPrivateAVFIOS_h
