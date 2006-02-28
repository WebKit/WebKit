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
#ifndef HTML_HTMLTextAreaElementImpl_H
#define HTML_HTMLTextAreaElementImpl_H

#include "HTMLGenericFormElementImpl.h"

namespace khtml {
    class RenderTextArea;
}

namespace DOM {

class HTMLTextAreaElementImpl : public HTMLGenericFormElementImpl
{
    friend class khtml::RenderTextArea;

public:
    enum WrapMethod {
        ta_NoWrap,
        ta_Virtual,
        ta_Physical
    };

    HTMLTextAreaElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f = 0);
    ~HTMLTextAreaElementImpl();

    virtual bool checkDTD(const NodeImpl* newChild) { return newChild->isTextNode(); }

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }

    WrapMethod wrap() const { return m_wrap; }

    virtual bool isEnumeratable() const { return true; }

    DOMString type() const;

    virtual bool maintainsState() { return true; }
    virtual QString state();
    virtual void restoreState(QStringList &);

    int selectionStart();
    int selectionEnd();

    void setSelectionStart(int);
    void setSelectionEnd(int);

    void select (  );
    void setSelectionRange(int, int);

    virtual void childrenChanged();
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual bool appendFormData(FormDataList&, bool);
    virtual void reset();
    DOMString value();
    void setValue(const DOMString &value);
    DOMString defaultValue();
    void setDefaultValue(const DOMString &value);
    
    void invalidateValue() { m_valueMatchesRenderer = false; }
    void rendererWillBeDestroyed();

    virtual bool isEditable();
    
    virtual void accessKeyAction(bool sendToAnyElement);
    
    DOMString accessKey() const;
    void setAccessKey(const DOMString &);

    void setCols(int);

    void setRows(int);

protected:
    int m_rows;
    int m_cols;
    WrapMethod m_wrap;
    String m_value;
    bool m_valueMatchesRenderer;
    
private:
    void updateValue();
};

} //namespace

#endif
