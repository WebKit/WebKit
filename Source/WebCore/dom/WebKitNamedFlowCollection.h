/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef WebKitNamedFlowCollection_h
#define WebKitNamedFlowCollection_h

#include <wtf/Forward.h>
#include <wtf/ListHashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class Document;
class WebKitNamedFlow;

class WebKitNamedFlowCollection : public RefCounted<WebKitNamedFlowCollection> {
public:
    static PassRefPtr<WebKitNamedFlowCollection> create(Document* doc) { return adoptRef(new WebKitNamedFlowCollection(doc)); }

    Vector<RefPtr<WebKitNamedFlow> > namedFlows();
    WebKitNamedFlow* flowByName(const String&);
    PassRefPtr<WebKitNamedFlow> ensureFlowWithName(const String&);

    void discardNamedFlow(WebKitNamedFlow*);

    void documentDestroyed();

    Document* document() const { return m_document; }

private:
    explicit WebKitNamedFlowCollection(Document*);

    Document* m_document;

    struct NamedFlowHashFunctions;
    struct NamedFlowHashTranslator;

    typedef ListHashSet<WebKitNamedFlow*, 1, NamedFlowHashFunctions> NamedFlowSet;

    NamedFlowSet m_namedFlows;
};

} // namespace WebCore

#endif // WebKitNamedFlowCollection_h
