/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef HTML_HTMLSelectElementImpl_H
#define HTML_HTMLSelectElementImpl_H

#include "Array.h"
#include "HTMLGenericFormElementImpl.h"

namespace WebCore {

class HTMLOptionElementImpl;
class HTMLOptionsCollectionImpl;
class RenderSelect;

class HTMLSelectElementImpl : public HTMLGenericFormElementImpl {
    friend class RenderSelect;

public:
    HTMLSelectElementImpl(DocumentImpl*, HTMLFormElementImpl* = 0);
    HTMLSelectElementImpl(const QualifiedName& tagName, DocumentImpl*, HTMLFormElementImpl* = 0);
    ~HTMLSelectElementImpl();

    virtual int tagPriority() const { return 6; }
    virtual bool checkDTD(const NodeImpl* newChild);

    DOMString type() const;

    virtual void recalcStyle(StyleChange);

    int selectedIndex() const;
    void setSelectedIndex(int index);

    virtual bool isEnumeratable() const { return true; }

    int length() const;

    int minWidth() const { return m_minwidth; }

    int size() const { return m_size; }

    bool multiple() const { return m_multiple; }

    void add(HTMLElementImpl* element, HTMLElementImpl* before, ExceptionCode&);
    void remove(int index);

    DOMString value();
    void setValue(const DOMString&);
    
    PassRefPtr<HTMLOptionsCollectionImpl> options();

    virtual bool maintainsState() { return true; }
    virtual QString state();
    virtual void restoreState(QStringList &);

    virtual bool insertBefore(PassRefPtr<NodeImpl> newChild, NodeImpl* refChild, ExceptionCode&);
    virtual bool replaceChild(PassRefPtr<NodeImpl> newChild, NodeImpl* oldChild, ExceptionCode&);
    virtual bool removeChild(NodeImpl* child, ExceptionCode&);
    virtual bool appendChild(PassRefPtr<NodeImpl> newChild, ExceptionCode&);
    virtual ContainerNodeImpl* addChild(PassRefPtr<NodeImpl>);

    virtual void childrenChanged();

    virtual void parseMappedAttribute(MappedAttributeImpl*);

    virtual RenderObject* createRenderer(RenderArena*, RenderStyle *);
    virtual bool appendFormData(FormDataList&, bool);

    // get the actual listbox index of the optionIndexth option
    int optionToListIndex(int optionIndex) const;
    // reverse of optionToListIndex - get optionIndex from listboxIndex
    int listToOptionIndex(int listIndex) const;

    void setRecalcListItems();

    Array<HTMLElementImpl*> listItems() const {
        if (m_recalcListItems)
            const_cast<HTMLSelectElementImpl*>(this)->recalcListItems();
        return m_listItems;
    }
    virtual void reset();
    void notifyOptionSelected(HTMLOptionElementImpl* selectedOption, bool selected);

    virtual void defaultEventHandler(EventImpl*);
    virtual void accessKeyAction(bool sendToAnyElement);

    void setMultiple(bool);

    void setSize(int);

private:
    void recalcListItems();

    mutable Array<HTMLElementImpl*> m_listItems;
    int m_minwidth;
    int m_size;
    bool m_multiple;
    bool m_recalcListItems;
};

} //namespace

#endif
