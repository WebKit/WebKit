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

#include "HTMLGenericFormElementImpl.h"

#include <qmemarray.h>

namespace khtml {
    class RenderSelect;
};

namespace DOM {

class HTMLOptionElementImpl;
class HTMLOptionsCollectionImpl;

class HTMLSelectElementImpl : public HTMLGenericFormElementImpl
{
    friend class khtml::RenderSelect;

public:
    HTMLSelectElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f = 0);
    HTMLSelectElementImpl(const QualifiedName& tagName, DocumentImpl *doc, HTMLFormElementImpl *f = 0);
    ~HTMLSelectElementImpl();
    void init();

    virtual int tagPriority() const { return 6; }
    virtual bool checkDTD(const NodeImpl* newChild);

    DOMString type() const;

    virtual void recalcStyle( StyleChange );

    int selectedIndex() const;
    void setSelectedIndex( int index );

    virtual bool isEnumeratable() const { return true; }

    int length() const;

    int minWidth() const { return m_minwidth; }

    int size() const { return m_size; }

    bool multiple() const { return m_multiple; }

    void add ( HTMLElementImpl *element, HTMLElementImpl *before, int &exceptioncode );
    void remove ( int index );

    DOMString value();
    void setValue(const DOMString &);
    
    HTMLOptionsCollectionImpl *options();
    RefPtr<HTMLCollectionImpl> optionsHTMLCollection(); // FIXME: Remove this and migrate to options().

    virtual bool maintainsState() { return true; }
    virtual QString state();
    virtual void restoreState(QStringList &);

    virtual NodeImpl *insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode );
    virtual NodeImpl *replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *removeChild ( NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *appendChild ( NodeImpl *newChild, int &exceptioncode );
    virtual NodeImpl *addChild( NodeImpl* newChild );

    virtual void childrenChanged();

    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual bool appendFormData(FormDataList&, bool);

    // get the actual listbox index of the optionIndexth option
    int optionToListIndex(int optionIndex) const;
    // reverse of optionToListIndex - get optionIndex from listboxIndex
    int listToOptionIndex(int listIndex) const;

    void setRecalcListItems();

    QMemArray<HTMLElementImpl*> listItems() const
     {
         if (m_recalcListItems) const_cast<HTMLSelectElementImpl*>(this)->recalcListItems();
         return m_listItems;
     }
    virtual void reset();
    void notifyOptionSelected(HTMLOptionElementImpl *selectedOption, bool selected);

    virtual void defaultEventHandler(EventImpl *evt);
    virtual void accessKeyAction(bool sendToAnyElement);

    void setMultiple(bool);

    void setSize(int);

private:
    void recalcListItems();

protected:
    mutable QMemArray<HTMLElementImpl*> m_listItems;
    HTMLOptionsCollectionImpl *m_options;
    short m_minwidth;
    short m_size;
    bool m_multiple;
    bool m_recalcListItems;
};

} //namespace

#endif
