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

#include "HTMLElement.h"
#include "HTMLCollection.h" 

namespace WebCore {
    class FormData;
};

namespace WebCore {

class HTMLGenericFormElement;
class HTMLImageElement;
class HTMLFormCollection;

class HTMLFormElement : public HTMLElement
{
public:
    HTMLFormElement(Document *doc);
    virtual ~HTMLFormElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 3; }

    virtual void attach();
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
 
    RefPtr<HTMLCollection> elements();
    int length() const;

    String enctype() const { return m_enctype; }
    void setEnctype(const String &);

    String boundary() const { return m_boundary; }
    void setBoundary(const String &);

    bool autoComplete() const { return m_autocomplete; }

    virtual void parseMappedAttribute(MappedAttribute *attr);

    void registerFormElement(HTMLGenericFormElement *);
    void removeFormElement(HTMLGenericFormElement *);
    void registerImgElement(HTMLImageElement *);
    void removeImgElement(HTMLImageElement *);

    bool prepareSubmit();
    void submit(bool activateSubmitButton = false);
    void reset();

    void setMalformed(bool malformed) { m_malformed = malformed; }
    virtual bool isMalformed() { return m_malformed; }
    
    void setPreserveAcrossRemove(bool b) { m_preserveAcrossRemove = b; }
    bool preserveAcrossRemove() const { return m_preserveAcrossRemove; }

    virtual bool isURLAttribute(Attribute *attr) const;
    
    void submitClick();
    bool formWouldHaveSecureSubmission(const String &url);

    String name() const;
    void setName(const String &);

    String acceptCharset() const;
    void setAcceptCharset(const String &);

    String action() const;
    void setAction(const String &);

    String method() const;
    void setMethod(const String &);

    String target() const;
    void setTarget(const String &);

    static void i18nData();

    friend class HTMLFormCollection;

    HTMLCollection::CollectionInfo *collectionInfo;

    Vector<HTMLGenericFormElement*> formElements;
    Vector<HTMLImageElement*> imgElements;
    String m_url;
    String m_target;
    String m_enctype;
    String m_boundary;
    String m_acceptcharset;
    bool m_post : 1;
    bool m_multipart : 1;
    bool m_autocomplete : 1;
    bool m_insubmit : 1;
    bool m_doingsubmit : 1;
    bool m_inreset : 1;
    bool m_malformed : 1;
    bool m_preserveAcrossRemove : 1;

private:
    void parseEnctype(const String &);
    bool formData(WebCore::FormData &) const;

    unsigned formElementIndex(HTMLGenericFormElement *);

    String oldNameAttr;
};

} //namespace

#endif
