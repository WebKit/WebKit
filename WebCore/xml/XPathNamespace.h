/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XPathNamespace_h
#define XPathNamespace_h

#if ENABLE(XPATH)

#include "AtomicString.h"
#include "Node.h"

namespace WebCore {

    // FIXME: This class is never instantiated. Maybe it should be removed.

    class XPathNamespace : public Node {
    private:
        XPathNamespace(PassRefPtr<Element> ownerElement, const AtomicString& prefix, const AtomicString& uri);

        virtual Document* ownerDocument() const;
        virtual Element* ownerElement() const;

        virtual const AtomicString& prefix() const;
        virtual String nodeName() const;
        virtual String nodeValue() const;
        virtual const AtomicString& namespaceURI() const;

        virtual NodeType nodeType() const;

        RefPtr<Element> m_ownerElement;
        AtomicString m_prefix;
        AtomicString m_uri;
    };

}

#endif // ENABLE(XPATH)

#endif // XPathNamespace_h

