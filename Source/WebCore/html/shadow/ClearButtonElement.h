/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ClearButtonElement_h
#define ClearButtonElement_h

#include "HTMLDivElement.h"
#include <wtf/Forward.h>

namespace WebCore {

class ClearButtonElement : public HTMLDivElement {
public:
    class ClearButtonOwner {
    public:
        virtual ~ClearButtonOwner() { }
        virtual void focusAndSelectClearButtonOwner() = 0;
        virtual bool shouldClearButtonRespondToMouseEvents() = 0;
        virtual void clearValue() = 0;
    };

    static PassRefPtr<ClearButtonElement> create(Document*, ClearButtonOwner&);
    void releaseCapture();
    void removeClearButtonOwner() { m_clearButtonOwner = 0; }

private:
    ClearButtonElement(Document*, ClearButtonOwner&);
    virtual void detach();
    virtual bool isMouseFocusable() const { return false; }
    virtual void defaultEventHandler(Event*);

    ClearButtonOwner* m_clearButtonOwner;
    bool m_capturing;
};

} // namespace

#endif // ClearButtonElement_h
