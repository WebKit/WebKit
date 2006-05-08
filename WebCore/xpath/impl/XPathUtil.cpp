/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
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
#include "config.h"

#if XPATH_SUPPORT

#include "XPathUtil.h"
#include "Node.h"

namespace WebCore {
namespace XPath {
        
bool isRootDomNode(Node* node)
{
    return node && !node->parentNode();
}

String stringValue(Node* node)
{
    switch (node->nodeType()) {
        case Node::ATTRIBUTE_NODE:
        case Node::PROCESSING_INSTRUCTION_NODE:
        case Node::COMMENT_NODE:
        case Node::TEXT_NODE:
            return node->nodeValue();
        default:
            if (isRootDomNode(node)
                 || node->nodeType() == Node::ELEMENT_NODE) {
                String str;
                
                for (Node* n = node->firstChild(); n; n = n->traverseNextNode(node))
                    str += stringValue(n);

                return str;
            }
    }
    
    return String();
}

bool isValidContextNode(Node* node)
{
    return node && (
           node->nodeType() == Node::ELEMENT_NODE ||
           node->nodeType() == Node::ATTRIBUTE_NODE ||
           node->nodeType() == Node::TEXT_NODE ||
           node->nodeType() == Node::CDATA_SECTION_NODE ||
           node->nodeType() == Node::PROCESSING_INSTRUCTION_NODE ||
           node->nodeType() == Node::COMMENT_NODE ||
           node->nodeType() == Node::DOCUMENT_NODE ||
           node->nodeType() == Node::XPATH_NAMESPACE_NODE);
}

}
}

#endif // XPATH_SUPPORT

