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

#include "HTMLGenericFormElement.h"

namespace WebCore {

class RenderTextArea;

class HTMLTextAreaElement : public HTMLGenericFormElement {
    friend class RenderTextArea;

public:
    enum WrapMethod { ta_NoWrap, ta_Virtual, ta_Physical };

    HTMLTextAreaElement(Document*, HTMLFormElement* = 0);
    ~HTMLTextAreaElement();

    virtual bool checkDTD(const Node* newChild) { return newChild->isTextNode(); }

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }

    WrapMethod wrap() const { return m_wrap; }

    virtual bool isEnumeratable() const { return true; }

    virtual const AtomicString& type() const;

    virtual String stateValue() const;
    virtual void restoreState(const String&);

    bool readOnly() const { return isReadOnlyControl(); }

    int selectionStart();
    int selectionEnd();

    void setSelectionStart(int);
    void setSelectionEnd(int);

    void select();
    void setSelectionRange(int, int);

    virtual void childrenChanged();
    virtual void parseMappedAttribute(MappedAttribute*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual bool appendFormData(FormDataList&, bool);
    virtual void reset();
    String value() const;
    void setValue(const String&);
    String defaultValue() const;
    void setDefaultValue(const String&);
    
    void invalidateValue() { m_valueMatchesRenderer = false; }
    void rendererWillBeDestroyed();
    
    virtual void accessKeyAction(bool sendToAnyElement);
    
    String accessKey() const;
    void setAccessKey(const String&);

    void setCols(int);
    void setRows(int);

private:
    void updateValue() const;

    int m_rows;
    int m_cols;
    WrapMethod m_wrap;
    mutable String m_value;
    mutable bool m_valueMatchesRenderer;
};

} //namespace

#endif
