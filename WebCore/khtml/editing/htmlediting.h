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

#ifndef __htmlediting_h__
#define __htmlediting_h__

#include "edit_command.h"
#include "composite_edit_command.h"
#include "apply_style_command.h"
#include "delete_selection_command.h"
#include "move_selection_command.h"
#include "replace_selection_command.h"
#include "typing_command.h"

#include "dom_nodeimpl.h"
#include "editing/edit_actions.h"
#include "qmap.h"
#include "qptrlist.h"
#include "qvaluelist.h"
#include "shared.h"

#define NON_BREAKING_SPACE 0xa0

namespace DOM {
    class CSSMutableStyleDeclarationImpl;
    class CSSProperty;
    class CSSStyleDeclarationImpl;
    class DocumentFragmentImpl;
    class HTMLElementImpl;
    class TextImpl;
}

namespace khtml {

void rebalanceWhitespaceInTextNode(DOM::NodeImpl *node, unsigned int start, unsigned int length);
DOM::DOMString &nonBreakingSpaceString();
void derefNodesInList(QPtrList<DOM::NodeImpl> &list);

//------------------------------------------------------------------------------------------

bool isSpecialElement(const DOM::NodeImpl *n);

DOM::ElementImpl *createDefaultParagraphElement(DOM::DocumentImpl *document);
DOM::ElementImpl *createBreakElement(DOM::DocumentImpl *document);

bool isTabSpanNode(const DOM::NodeImpl *node);
bool isTabSpanTextNode(const DOM::NodeImpl *node);
DOM::Position positionBeforeTabSpan(const DOM::Position& pos);
DOM::ElementImpl *createTabSpanElement(DOM::DocumentImpl *document, DOM::NodeImpl *tabTextNode=0);
DOM::ElementImpl *createTabSpanElement(DOM::DocumentImpl *document, QString *tabText);

bool isNodeRendered(const DOM::NodeImpl *);
bool isMailBlockquote(const DOM::NodeImpl *);
DOM::NodeImpl *nearestMailBlockquote(const DOM::NodeImpl *);

//------------------------------------------------------------------------------------------

bool isTableStructureNode(const DOM::NodeImpl *node);
DOM::ElementImpl *createBlockPlaceholderElement(DOM::DocumentImpl *document);

bool isFirstVisiblePositionInSpecialElement(const DOM::Position& pos);
DOM::Position positionBeforeContainingSpecialElement(const DOM::Position& pos);
bool isLastVisiblePositionInSpecialElement(const DOM::Position& pos);
DOM::Position positionAfterContainingSpecialElement(const DOM::Position& pos);
DOM::Position positionOutsideContainingSpecialElement(const DOM::Position &pos);

} // end namespace khtml

#endif
