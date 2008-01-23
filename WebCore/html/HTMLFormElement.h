/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLFormElement_h
#define HTMLFormElement_h

#include "HTMLCollection.h" 
#include "HTMLElement.h"

#include <wtf/OwnPtr.h>

namespace WebCore {

class Event;
class FormData;
class HTMLGenericFormElement;
class HTMLImageElement;
class HTMLInputElement;
class HTMLFormCollection;
class TextEncoding;

class HTMLFormElement : public HTMLElement {
public:
    HTMLFormElement(Document*);
    virtual ~HTMLFormElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 3; }

    virtual void attach();
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
 
    virtual void handleLocalEvents(Event*, bool useCapture);
     
    PassRefPtr<HTMLCollection> elements();
    void getNamedElements(const AtomicString&, Vector<RefPtr<Node> >&);
    
    unsigned length() const;
    Node* item(unsigned index);

    String enctype() const { return m_enctype; }
    void setEnctype(const String&);

    String encoding() const { return m_enctype; }
    void setEncoding(const String& enctype) { setEnctype(enctype); }

    bool autoComplete() const { return m_autocomplete; }

    virtual void parseMappedAttribute(MappedAttribute*);

    void registerFormElement(HTMLGenericFormElement*);
    void removeFormElement(HTMLGenericFormElement*);
    void registerImgElement(HTMLImageElement*);
    void removeImgElement(HTMLImageElement*);

    bool prepareSubmit(Event*);
    void submit();
    void submit(Event*, bool activateSubmitButton = false);
    void reset();

    void setMalformed(bool malformed) { m_malformed = malformed; }
    virtual bool isMalformed() { return m_malformed; }

    virtual bool isURLAttribute(Attribute*) const;
    
    void submitClick(Event*);
    bool formWouldHaveSecureSubmission(const String& url);

    String name() const;
    void setName(const String&);

    String acceptCharset() const;
    void setAcceptCharset(const String&);

    String action() const;
    void setAction(const String&);

    String method() const;
    void setMethod(const String&);

    virtual String target() const;
    void setTarget(const String&);
    
    PassRefPtr<HTMLGenericFormElement> elementForAlias(const AtomicString&);
    void addElementAlias(HTMLGenericFormElement*, const AtomicString& alias);

    // FIXME: Change this to be private after getting rid of all the clients.
    Vector<HTMLGenericFormElement*> formElements;

    class CheckedRadioButtons {
    public:
        void addButton(HTMLGenericFormElement*);
        void removeButton(HTMLGenericFormElement*);
        HTMLInputElement* checkedButtonForGroup(const AtomicString& name) const;

    private:
        typedef HashMap<AtomicStringImpl*, HTMLInputElement*> NameToInputMap;
        OwnPtr<NameToInputMap> m_nameToCheckedRadioButtonMap;
    };
    
    CheckedRadioButtons& checkedRadioButtons() { return m_checkedRadioButtons; }
    
private:
    void parseEnctype(const String&);
    bool isMailtoForm() const;
    TextEncoding dataEncoding() const;
    PassRefPtr<FormData> formData(const char* boundary) const;
    unsigned formElementIndex(HTMLGenericFormElement*);

    friend class HTMLFormCollection;

    typedef HashMap<RefPtr<AtomicStringImpl>, RefPtr<HTMLGenericFormElement> > AliasMap;
    
    AliasMap* m_elementAliases;
    HTMLCollection::CollectionInfo* collectionInfo;

    CheckedRadioButtons m_checkedRadioButtons;
    
    Vector<HTMLImageElement*> imgElements;
    String m_url;
    String m_target;
    String m_enctype;
    String m_acceptcharset;
    bool m_post : 1;
    bool m_multipart : 1;
    bool m_autocomplete : 1;
    bool m_insubmit : 1;
    bool m_doingsubmit : 1;
    bool m_inreset : 1;
    bool m_malformed : 1;
    String oldNameAttr;
};

} // namespace WebCore

#endif // HTMLFormElement_h
