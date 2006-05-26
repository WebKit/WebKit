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

#ifndef __modify_selection_list_level_h__
#define __modify_selection_list_level_h__

#include "CompositeEditCommand.h"

namespace WebCore {

// ModifySelectionListLevelCommand provides functions useful for both increasing and decreasing the list
// level.  So, it is the base class of IncreaseSelectionListLevelCommand and DecreaseSelectionListLevelCommand.
// It is not used on its own.
class ModifySelectionListLevelCommand : public CompositeEditCommand
{
public:
    ModifySelectionListLevelCommand(WebCore::Document* document);
    
private:
    virtual bool preservesTypingStyle() const;
    
protected:
    void appendSiblingNodeRange(WebCore::Node* startNode, WebCore::Node* endNode, WebCore::Node* newParent);
    void insertSiblingNodeRangeBefore(WebCore::Node* startNode, WebCore::Node* endNode, WebCore::Node* refNode);
    void insertSiblingNodeRangeAfter(WebCore::Node* startNode, WebCore::Node* endNode, WebCore::Node* refNode);
};

// IncreaseSelectionListLevelCommand moves the selected list items one level deeper
typedef enum EListType { InheritedListType, OrderedList, UnorderedList };

class IncreaseSelectionListLevelCommand : public ModifySelectionListLevelCommand
{
public:
    static bool canIncreaseSelectionListLevel(WebCore::Document*);
    static WebCore::Node* increaseSelectionListLevel(WebCore::Document*);
    static WebCore::Node* increaseSelectionListLevelOrdered(WebCore::Document*);
    static WebCore::Node* increaseSelectionListLevelUnordered(WebCore::Document*);

    IncreaseSelectionListLevelCommand(WebCore::Document* document, EListType);
    Node*       listElement();
    virtual void doApply();

private:
    EListType   m_listType;
    Node*       m_listElement;
};

// DecreaseSelectionListLevelCommand moves the selected list items one level shallower
class DecreaseSelectionListLevelCommand : public ModifySelectionListLevelCommand
{
public:
    static bool canDecreaseSelectionListLevel(WebCore::Document*);
    static void decreaseSelectionListLevel(WebCore::Document*);

    DecreaseSelectionListLevelCommand(WebCore::Document* document);

    virtual void doApply();
};

} // namespace WebCore

#endif // __modify_selection_list_level_h__
