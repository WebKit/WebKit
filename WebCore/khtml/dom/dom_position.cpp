/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#include "dom_position.h"
#include "xml/dom_nodeimpl.h"

using DOM::DOMPosition;

DOMPosition::DOMPosition(NodeImpl *node, long offset) 
    : m_node(0), m_offset(offset) 
{ 
    if (node) {
        m_node = node;
        m_node->ref();
    }
};

DOMPosition::DOMPosition(const DOMPosition &o)
    : m_node(0), m_offset(o.offset()) 
{
    if (o.node()) {
        m_node = o.node();
        m_node->ref();
    }
}

DOMPosition::~DOMPosition() {
    if (m_node) {
        m_node->deref();
    }
}

DOMPosition &DOMPosition::operator=(const DOMPosition &o)
{
    if (m_node) {
        m_node->deref();
    }
    m_node = o.node();
    if (m_node) {
        m_node->ref();
    }

    m_offset = o.offset();
    
    return *this;
}
