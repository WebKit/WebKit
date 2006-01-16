/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"

#ifndef KHTML_NO_XBL

#include "xbl_docimpl.h"
#include "xbl_tokenizer.h"
#include "xbl_protobinding.h"

using DOM::DocumentImpl;
using khtml::XMLHandler;

namespace XBL {

XBLDocumentImpl::XBLDocumentImpl()
:DocumentImpl(0,0)
{
    m_prototypeBindingTable.setAutoDelete(true); // The prototype bindings will be deleted when the XBL document dies.
}

XBLDocumentImpl::~XBLDocumentImpl()
{
}

XMLHandler* XBLDocumentImpl::createTokenHandler()
{
    return new XBLTokenHandler(docPtr());
}

void XBLDocumentImpl::setPrototypeBinding(const DOM::DOMString& id, XBLPrototypeBinding* binding)
{
    m_prototypeBindingTable.replace(id.qstring(), binding);
}

XBLPrototypeBinding* XBLDocumentImpl::prototypeBinding(const DOM::DOMString& id)
{
    if (id.length() == 0)
        return 0;
    
    return m_prototypeBindingTable.find(id.qstring());
}

}

#endif
