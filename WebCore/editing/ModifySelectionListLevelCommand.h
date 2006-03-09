/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef __modify_selection_list_level_command_h__
#define __modify_selection_list_level_command_h__

#include "CompositeEditCommand.h"

namespace khtml {

enum EListLevelModification { DecreaseListLevel, IncreaseListLevel };

class ModifySelectionListLevelCommand : public CompositeEditCommand
{
public:

    static bool canIncreaseSelectionListLevel(DOM::DocumentImpl*);
    static bool canDecreaseSelectionListLevel(DOM::DocumentImpl*);
    static void increaseSelectionListLevel(DOM::DocumentImpl*);
    static void decreaseSelectionListLevel(DOM::DocumentImpl*);

    ModifySelectionListLevelCommand(DOM::DocumentImpl* document, EListLevelModification);

    virtual void doApply();

    
private:
    EListLevelModification  m_modification;
    
    virtual bool preservesTypingStyle() const;
    
    // utility functions
    void appendSiblingNodeRange(NodeImpl* startNode, NodeImpl* endNode, NodeImpl* newParent);
    void insertSiblingNodeRangeBefore(NodeImpl* startNode, NodeImpl* endNode, NodeImpl* refNode);
    void insertSiblingNodeRangeAfter(NodeImpl* startNode, NodeImpl* endNode, NodeImpl* refNode);
    
    // main functionality
    void increaseListLevel(const Selection&);
    void decreaseListLevel(const Selection&);
};

} // namespace khtml

#endif // __modify_selection_list_level_command_h__
