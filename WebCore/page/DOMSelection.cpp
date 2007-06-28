/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "DOMSelection.h"

#include "Frame.h"
#include "Node.h"
#include "PlatformString.h"
#include "Range.h"
#include "SelectionController.h"

namespace WebCore {

DOMSelection::DOMSelection(Frame* frame)
    : m_frame(frame)
{
}

Frame* DOMSelection::frame() const
{
    return m_frame;
}

void DOMSelection::disconnectFrame()
{
    m_frame = 0;
}

Node* DOMSelection::anchorNode() const
{
    if (!m_frame)
        return 0;
    return m_frame->selectionController()->anchorNode();
}

Node* DOMSelection::baseNode() const
{
    if (!m_frame)
        return 0;
    return m_frame->selectionController()->baseNode();
}

int DOMSelection::anchorOffset() const
{
    if (!m_frame)
        return 0;
    return m_frame->selectionController()->anchorOffset();
}

int DOMSelection::baseOffset() const
{
    if (!m_frame)
        return 0;
    return m_frame->selectionController()->baseOffset();
}

Node* DOMSelection::focusNode() const
{
    if (!m_frame)
        return 0;
    return m_frame->selectionController()->focusNode();
}

Node* DOMSelection::extentNode() const
{
    if (!m_frame)
        return 0;
    return m_frame->selectionController()->extentNode();
}

int DOMSelection::focusOffset() const
{
    if (!m_frame)
        return 0;
    return m_frame->selectionController()->focusOffset();
}

int DOMSelection::extentOffset() const
{
    if (!m_frame)
        return 0;
    return m_frame->selectionController()->extentOffset();
}

bool DOMSelection::isCollapsed() const
{
    if (!m_frame)
        return false;
    return m_frame->selectionController()->isCollapsed();
}

String DOMSelection::type() const
{
    if (!m_frame)
        return String();
    return m_frame->selectionController()->type();
}

int DOMSelection::rangeCount() const
{
    if (!m_frame)
        return 0;
    return m_frame->selectionController()->rangeCount();
}

void DOMSelection::collapse(Node* node, int offset, ExceptionCode& ec)
{
    if (!m_frame)
        return;
    m_frame->selectionController()->collapse(node, offset, ec);
}

void DOMSelection::collapseToEnd()
{
    if (!m_frame)
        return;
    m_frame->selectionController()->collapseToEnd();
}

void DOMSelection::collapseToStart()
{
    if (!m_frame)
        return;
    m_frame->selectionController()->collapseToStart();
}

void DOMSelection::empty()
{
    if (!m_frame)
        return;
    m_frame->selectionController()->empty();
}

void DOMSelection::setBaseAndExtent(Node* baseNode, int baseOffset, Node* extentNode, int extentOffset, ExceptionCode& ec)
{
    if (!m_frame)
        return;
    m_frame->selectionController()->setBaseAndExtent(baseNode, baseOffset, extentNode, extentOffset, ec);
}

void DOMSelection::setPosition(Node* node, int offset, ExceptionCode& ec)
{
    if (!m_frame)
        return;
    m_frame->selectionController()->setPosition(node, offset, ec);
}

void DOMSelection::setPosition(Node* node, ExceptionCode& ec)
{
    if (!m_frame)
        return;
    m_frame->selectionController()->setPosition(node, 0, ec);
}

void DOMSelection::modify(const String& alter, const String& direction, const String& granularity)
{
    if (!m_frame)
        return;
    m_frame->selectionController()->modify(alter, direction, granularity);
}

PassRefPtr<Range> DOMSelection::getRangeAt(int index, ExceptionCode& ec)
{
    if (!m_frame)
        return 0;
    return m_frame->selectionController()->getRangeAt(index, ec);
}

void DOMSelection::removeAllRanges()
{
    if (!m_frame)
        return;
    m_frame->selectionController()->removeAllRanges();
}

void DOMSelection::addRange(Range* range)
{
    if (!m_frame)
        return;
    m_frame->selectionController()->addRange(range);
}

String DOMSelection::toString()
{
    if (!m_frame)
        return String();
    return m_frame->selectionController()->toString();
}

} // namespace WebCore
