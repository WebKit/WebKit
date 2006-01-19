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
#ifndef HTML_HTMLFormElementImpl_H
#define HTML_HTMLFormElementImpl_H

#include "HTMLElementImpl.h"
#include "HTMLCollectionImpl.h" 

#include <qptrvector.h>

namespace khtml {
    class FormData;
};

namespace DOM {

class HTMLGenericFormElementImpl;
class HTMLImageElementImpl;
class HTMLFormCollectionImpl;

class HTMLFormElementImpl : public HTMLElementImpl
{
public:
    HTMLFormElementImpl(DocumentImpl *doc);
    virtual ~HTMLFormElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 3; }

    virtual void attach();
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
 
    RefPtr<HTMLCollectionImpl> elements();
    int length() const;

    DOMString enctype() const { return m_enctype; }
    void setEnctype(const DOMString &);

    DOMString boundary() const { return m_boundary; }
    void setBoundary(const DOMString &);

    bool autoComplete() const { return m_autocomplete; }

    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    void registerFormElement(HTMLGenericFormElementImpl *);
    void removeFormElement(HTMLGenericFormElementImpl *);
    void registerImgElement(HTMLImageElementImpl *);
    void removeImgElement(HTMLImageElementImpl *);

    bool prepareSubmit();
    void submit(bool activateSubmitButton = false);
    void reset();

    void setMalformed(bool malformed) { m_malformed = malformed; }
    virtual bool isMalformed() { return m_malformed; }
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;
    
    void submitClick();
    bool formWouldHaveSecureSubmission(const DOMString &url);

    DOMString name() const;
    void setName(const DOMString &);

    DOMString acceptCharset() const;
    void setAcceptCharset(const DOMString &);

    DOMString action() const;
    void setAction(const DOMString &);

    DOMString method() const;
    void setMethod(const DOMString &);

    DOMString target() const;
    void setTarget(const DOMString &);

    static void i18nData();

    friend class HTMLFormElement;
    friend class HTMLFormCollectionImpl;

    HTMLCollectionImpl::CollectionInfo *collectionInfo;

    QPtrVector<HTMLGenericFormElementImpl> formElements;
    QPtrVector<HTMLImageElementImpl> imgElements;
    DOMString m_url;
    DOMString m_target;
    DOMString m_enctype;
    DOMString m_boundary;
    DOMString m_acceptcharset;
    bool m_post : 1;
    bool m_multipart : 1;
    bool m_autocomplete : 1;
    bool m_insubmit : 1;
    bool m_doingsubmit : 1;
    bool m_inreset : 1;
    bool m_malformed : 1;
    
private:
    void parseEnctype(const DOMString &);
    bool formData(khtml::FormData &) const;

    unsigned formElementIndex(HTMLGenericFormElementImpl *);

    DOMString oldNameAttr;
};

} //namespace

#endif
