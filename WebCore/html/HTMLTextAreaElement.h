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
}

namespace WebCore {

class HTMLTextAreaElement : public HTMLGenericFormElement
{
    friend class WebCore::RenderTextArea;

public:
    enum WrapMethod {
        ta_NoWrap,
        ta_Virtual,
        ta_Physical
    };

    HTMLTextAreaElement(Document *doc, HTMLFormElement *f = 0);
    ~HTMLTextAreaElement();

    virtual bool checkDTD(const Node* newChild) { return newChild->isTextNode(); }

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }

    WrapMethod wrap() const { return m_wrap; }

    virtual bool isEnumeratable() const { return true; }

    String type() const;

    virtual bool maintainsState() { return true; }
    virtual DeprecatedString state();
    virtual void restoreState(DeprecatedStringList &);

    bool readOnly() const { return isReadOnlyControl(); }

    int selectionStart();
    int selectionEnd();

    void setSelectionStart(int);
    void setSelectionEnd(int);

    void select (  );
    void setSelectionRange(int, int);

    virtual void childrenChanged();
    virtual void parseMappedAttribute(MappedAttribute *attr);
    virtual WebCore::RenderObject *createRenderer(RenderArena *, WebCore::RenderStyle *);
    virtual bool appendFormData(FormDataList&, bool);
    virtual void reset();
    String value();
    void setValue(const String &value);
    String defaultValue();
    void setDefaultValue(const String &value);
    
    void invalidateValue() { m_valueMatchesRenderer = false; }
    void rendererWillBeDestroyed();
    
    virtual void accessKeyAction(bool sendToAnyElement);
    
    String accessKey() const;
    void setAccessKey(const String &);

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
