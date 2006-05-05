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

#ifndef HTML_HTMLGenericFormElementImpl_h
#define HTML_HTMLGenericFormElementImpl_h

#include "HTMLElement.h"

namespace WebCore {

class FormDataList;
class HTMLFormElement;
class RenderFormElement;

class HTMLGenericFormElement : public HTMLElement
{
    friend class HTMLFormElement;
    friend class RenderFormElement;

public:
    HTMLGenericFormElement(const QualifiedName& tagName, Document*, HTMLFormElement* = 0);
    virtual ~HTMLGenericFormElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    HTMLFormElement* form() const { return m_form; }

    virtual const AtomicString& type() const = 0;

    virtual bool isControl() const { return true; }
    virtual bool isEnabled() const { return !disabled(); }

    virtual void parseMappedAttribute(MappedAttribute*);
    virtual void attach();
    virtual void insertedIntoTree(bool deep);
    virtual void removedFromTree(bool deep);
    virtual void closeRenderer();

    virtual void reset() {}

    void onSelect();
    void onChange();

    bool disabled() const;
    void setDisabled(bool);

    virtual bool isFocusable() const;
    virtual bool isKeyboardFocusable() const;
    virtual bool isMouseFocusable() const;
    virtual bool isEnumeratable() const { return false; }

    virtual bool isReadOnlyControl() const { return m_readOnly; }
    void setReadOnly(bool);

    virtual void recalcStyle(StyleChange);

    virtual const AtomicString& name() const;
    void setName(const AtomicString& name);

    virtual bool isGenericFormElement() const { return true; }
    virtual bool isRadioButton() const { return false; }

    /* Override in derived classes to get the encoded name=value pair for submitting.
     * Return true for a successful control (see HTML4-17.13.2).
     */
    virtual bool appendFormData(FormDataList&, bool) { return false; }

    virtual String stateValue() const;
    virtual void restoreState(const String& value);

    virtual bool isSuccessfulSubmitButton() const { return false; }
    virtual bool isActivatedSubmit() const { return false; }
    virtual void setActivatedSubmit(bool flag) { }

    int tabIndex() const;
    void setTabIndex(int);

protected:
    HTMLFormElement* getForm() const;

private:
    HTMLFormElement* m_form;
    bool m_disabled;
    bool m_readOnly;
};

} //namespace

#endif
