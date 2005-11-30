/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef KHTML_EDITING_MARKUP_H
#define KHTML_EDITING_MARKUP_H

#include <xml/dom_docimpl.h>
#include "html_interchange.h"

namespace khtml {

enum EChildrenOnly { IncludeNode, ChildrenOnly };

DOM::DocumentFragmentImpl *createFragmentFromText(DOM::DocumentImpl *document, const QString &text);
DOM::DocumentFragmentImpl *createFragmentFromMarkup(DOM::DocumentImpl *document, const QString &markup, const QString &baseURL);
DOM::DocumentFragmentImpl *createFragmentFromNodeList(DOM::DocumentImpl *document, const QPtrList<DOM::NodeImpl> &nodeList);

QString createMarkup(const DOM::RangeImpl *range,
    QPtrList<DOM::NodeImpl> *nodes = 0, EAnnotateForInterchange = DoNotAnnotateForInterchange);
QString createMarkup(const DOM::NodeImpl *node, EChildrenOnly = IncludeNode,
    QPtrList<DOM::NodeImpl> *nodes = 0, EAnnotateForInterchange = DoNotAnnotateForInterchange);

}

#endif // KHTML_EDITING_MARKUP_H
