/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MockCCQuadCuller_h
#define MockCCQuadCuller_h

#include "CCDrawQuad.h"
#include "CCQuadSink.h"
#include "IntRect.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class MockCCQuadCuller : public CCQuadSink {
public:
    MockCCQuadCuller()
        : m_activeQuadList(m_quadListStorage)
        , m_activeSharedQuadStateList(m_sharedQuadStateStorage)
    { }

    explicit MockCCQuadCuller(CCQuadList& externalQuadList, CCSharedQuadStateList& externalSharedQuadStateList)
        : m_activeQuadList(externalQuadList)
        , m_activeSharedQuadStateList(externalSharedQuadStateList)
    { }

    virtual bool append(WTF::PassOwnPtr<CCDrawQuad> newQuad) OVERRIDE
    {
        OwnPtr<CCDrawQuad> drawQuad = newQuad;
        if (!drawQuad->quadRect().isEmpty()) {
            m_activeQuadList.append(drawQuad.release());
            return true;
        }
        return false;
    }

    virtual CCSharedQuadState* useSharedQuadState(PassOwnPtr<CCSharedQuadState> passSharedQuadState) OVERRIDE
    {
        OwnPtr<CCSharedQuadState> sharedQuadState(passSharedQuadState);
        sharedQuadState->id = m_activeSharedQuadStateList.size();

        CCSharedQuadState* rawPtr = sharedQuadState.get();
        m_activeSharedQuadStateList.append(sharedQuadState.release());
        return rawPtr;
    }

    const CCQuadList& quadList() const { return m_activeQuadList; };
    const CCSharedQuadStateList& sharedQuadStateList() const { return m_activeSharedQuadStateList; };

private:
    CCQuadList& m_activeQuadList;
    CCQuadList m_quadListStorage;
    CCSharedQuadStateList& m_activeSharedQuadStateList;
    CCSharedQuadStateList m_sharedQuadStateStorage;
};

} // namespace WebCore
#endif // MockCCQuadCuller_h
