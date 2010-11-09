/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef FindPageOverlay_h
#define FindPageOverlay_h

#include "PageOverlay.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebKit {

class FindController;

class FindPageOverlay : public PageOverlay, public PageOverlay::Client {
public:
    static PassOwnPtr<FindPageOverlay> create(FindController*);
    virtual ~FindPageOverlay();

private:
    explicit FindPageOverlay(FindController*);

    Vector<WebCore::IntRect> rectsForTextMatches();
    WebCore::IntRect bounds() const;

    // PageOverlay::Client.
    virtual void drawRect(PageOverlay*, WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect);
    virtual bool mouseEvent(PageOverlay*, const WebMouseEvent&);

    FindController* m_findController;
};

} // namespace WebKit

#endif // FindPageOverlay_h
