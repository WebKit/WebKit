/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "CheckedRadioButtons.h"
#include "FormDataBuilder.h"
#include "FormState.h"
#include "HTMLElement.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class Event;
class FormData;
class HTMLFormControlElement;
class HTMLImageElement;
class HTMLInputElement;
class HTMLFormCollection;
class TextEncoding;

struct CollectionCache;

class HTMLFormElement : public HTMLElement { 
public:
    HTMLFormElement(const QualifiedName&, Document*);
    virtual ~HTMLFormElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 3; }

    virtual void attach();
    virtual bool rendererIsNeeded(RenderStyle*);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
 
    virtual void handleLocalEvents(Event*);
     
    PassRefPtr<HTMLCollection> elements();
    void getNamedElements(const AtomicString&, Vector<RefPtr<Node> >&);
    
    unsigned length() const;
    Node* item(unsigned index);

    String enctype() const { return m_formDataBuilder.encodingType(); }
    void setEnctype(const String&);

    String encoding() const { return m_formDataBuilder.encodingType(); }
    void setEncoding(const String& value) { setEnctype(value); }

    bool autoComplete() const { return m_autocomplete; }

    virtual void parseMappedAttribute(MappedAttribute*);

    void registerFormElement(HTMLFormControlElement*);
    void removeFormElement(HTMLFormControlElement*);
    void registerImgElement(HTMLImageElement*);
    void removeImgElement(HTMLImageElement*);

    bool prepareSubmit(Event*);
    void submit(Frame* javaScriptActiveFrame = 0);
    void reset();

    // Used to indicate a malformed state to keep from applying the bottom margin of the form.
    void setMalformed(bool malformed) { m_malformed = malformed; }
    bool isMalformed() const { return m_malformed; }

    void setDemoted(bool demoted) { m_demoted = demoted; }
    bool isDemoted() const { return m_demoted; }

    virtual bool isURLAttribute(Attribute*) const;
    
    void submitClick(Event*);
    bool formWouldHaveSecureSubmission(const String& url);

    String name() const;
    void setName(const String&);

    bool noValidate() const;
    void setNoValidate(bool);

    String acceptCharset() const { return m_formDataBuilder.acceptCharset(); }
    void setAcceptCharset(const String&);

    String action() const;
    void setAction(const String&);

    String method() const;
    void setMethod(const String&);

    virtual String target() const;
    void setTarget(const String&);

    HTMLFormControlElement* defaultButton() const;

    bool checkValidity();

    PassRefPtr<HTMLFormControlElement> elementForAlias(const AtomicString&);
    void addElementAlias(HTMLFormControlElement*, const AtomicString& alias);

    // FIXME: Change this to be private after getting rid of all the clients.
    Vector<HTMLFormControlElement*> formElements;

    CheckedRadioButtons& checkedRadioButtons() { return m_checkedRadioButtons; }
    
    virtual void documentDidBecomeActive();

protected:
    virtual void willMoveToNewOwnerDocument();
    virtual void didMoveToNewOwnerDocument();

private:
    void submit(Event*, bool activateSubmitButton, bool lockHistory, FormSubmissionTrigger);

    bool isMailtoForm() const;
    TextEncoding dataEncoding() const;
    PassRefPtr<FormData> createFormData();
    unsigned formElementIndex(HTMLFormControlElement*);

    friend class HTMLFormCollection;

    typedef HashMap<RefPtr<AtomicStringImpl>, RefPtr<HTMLFormControlElement> > AliasMap;

    FormDataBuilder m_formDataBuilder;
    AliasMap* m_elementAliases;
    CollectionCache* collectionInfo;

    CheckedRadioButtons m_checkedRadioButtons;
    
    Vector<HTMLImageElement*> imgElements;
    String m_url;
    String m_target;
    bool m_autocomplete : 1;
    bool m_insubmit : 1;
    bool m_doingsubmit : 1;
    bool m_inreset : 1;
    bool m_malformed : 1;
    bool m_demoted : 1;
    AtomicString m_name;
};

} // namespace WebCore

#endif // HTMLFormElement_h
