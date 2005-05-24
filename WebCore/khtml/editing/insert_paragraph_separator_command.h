/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#ifndef __insert_paragraph_separator_command_h__
#define __insert_paragraph_separator_command_h__

#include "composite_edit_command.h"
#include "qptrlist.h"

namespace khtml {

class InsertParagraphSeparatorCommand : public CompositeEditCommand
{
public:
    InsertParagraphSeparatorCommand(DOM::DocumentImpl *document);
    virtual ~InsertParagraphSeparatorCommand();

    virtual void doApply();

private:
    DOM::ElementImpl *createParagraphElement();
    void calculateStyleBeforeInsertion(const DOM::Position &);
    void applyStyleAfterInsertion();

    virtual bool preservesTypingStyle() const;

    QPtrList<DOM::NodeImpl> ancestors;
    QPtrList<DOM::NodeImpl> clonedNodes;
    DOM::CSSMutableStyleDeclarationImpl *m_style;
};

} // namespace khtml

#endif // __insert_paragraph_separator_command_h__
