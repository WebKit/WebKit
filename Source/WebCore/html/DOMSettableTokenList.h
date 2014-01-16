/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef DOMSettableTokenList_h
#define DOMSettableTokenList_h

#include "DOMTokenList.h"
#include "SpaceSplitString.h"
#include <wtf/RefCounted.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

typedef int ExceptionCode;

class DOMSettableTokenList : public DOMTokenList, public RefCounted<DOMSettableTokenList> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<DOMSettableTokenList> create();

    virtual void ref() override FINAL;
    virtual void deref() override FINAL;

    virtual unsigned length() const override FINAL;
    virtual const AtomicString item(unsigned index) const override FINAL;

    virtual AtomicString value() const override FINAL;
    virtual void setValue(const AtomicString&) override FINAL;

private:
    virtual bool containsInternal(const AtomicString&) const override FINAL;

    AtomicString m_value;
    SpaceSplitString m_tokens;
};

} // namespace WebCore

#endif // DOMSettableTokenList_h
