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
#ifndef HTML_HTMLButtonElementImpl_H
#define HTML_HTMLButtonElementImpl_H

#include "HTMLGenericFormElement.h"

namespace WebCore {

class HTMLButtonElement : public HTMLGenericFormElement
{
public:
    HTMLButtonElement(Document *doc, HTMLFormElement *f = 0);
    virtual ~HTMLButtonElement();

    enum typeEnum { SUBMIT, RESET, BUTTON };

    virtual const AtomicString& type() const;
        
    virtual WebCore::RenderObject *createRenderer(RenderArena*, WebCore::RenderStyle*);

    virtual void parseMappedAttribute(MappedAttribute *attr);
    virtual void defaultEventHandler(Event *evt);
    virtual bool appendFormData(FormDataList&, bool);

    virtual bool isEnumeratable() const { return true; } 

    virtual bool isSuccessfulSubmitButton() const;
    virtual bool isActivatedSubmit() const;
    virtual void setActivatedSubmit(bool flag);

    virtual void accessKeyAction(bool sendToAnyElement);

    String accessKey() const;
    void setAccessKey(const String &);

    String value() const;
    void setValue(const String &);
    
protected:
    String m_value;
    String m_currValue;
    unsigned m_type : 2; // typeEnum
    bool m_dirty : 1;
    bool m_activeSubmit : 1;
};

} //namespace

#endif
