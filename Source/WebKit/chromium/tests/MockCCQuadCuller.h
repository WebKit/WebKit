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

#include "IntRect.h"
#include "cc/CCDrawQuad.h"
#include "cc/CCQuadCuller.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class MockCCQuadCuller : public WebCore::CCQuadCuller {
public:
    MockCCQuadCuller()
        : CCQuadCuller(m_quadListStorage, 0, 0, false)
        , m_activeQuadList(m_quadListStorage)
    { }

    explicit MockCCQuadCuller(CCQuadList& externalQuadList)
        : CCQuadCuller(externalQuadList, 0, 0, false)
        , m_activeQuadList(externalQuadList)
    { }

    virtual bool append(WTF::PassOwnPtr<WebCore::CCDrawQuad> newQuad)
    {
        OwnPtr<WebCore::CCDrawQuad> drawQuad = newQuad;
        if (!drawQuad->quadRect().isEmpty()) {
            m_activeQuadList.append(drawQuad.release());
            return true;
        }
        return false;
    }

    const WebCore::CCQuadList& quadList() const { return m_activeQuadList; };

private:
    WebCore::CCQuadList& m_activeQuadList;
    WebCore::CCQuadList m_quadListStorage;
};

} // namespace WebCore
#endif // MockCCQuadCuller_h
