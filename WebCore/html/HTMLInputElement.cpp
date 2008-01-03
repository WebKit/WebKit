/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
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

#include "config.h"
#include "HTMLInputElement.h"

#include "BeforeTextInsertedEvent.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "Editor.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FocusController.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLFormElement.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "Page.h"
#include "RenderButton.h"
#include "RenderFileUploadControl.h"
#include "RenderImage.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderSlider.h"
#include "SelectionController.h"
#include "TextBreakIterator.h"
#include "TextEvent.h"
#include "TextIterator.h"

using namespace std;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

const int maxSavedResults = 256;

// FIXME: According to HTML4, the length attribute's value can be arbitrarily
// large. However, due to http://bugs.webkit.org/show_bugs.cgi?id=14536 things
// get rather sluggish when a text field has a larger number of characters than
// this, even when just clicking in the text field.
static const int cMaxLen = 524288;

static int numGraphemeClusters(StringImpl* s)
{
    if (!s)
        return 0;

    TextBreakIterator* it = characterBreakIterator(s->characters(), s->length());
    if (!it)
        return 0;
    int num = 0;
    while (textBreakNext(it) != TextBreakDone)
        ++num;
    return num;
}

static int numCharactersInGraphemeClusters(StringImpl* s, int numGraphemeClusters)
{
    if (!s)
        return 0;

    TextBreakIterator* it = characterBreakIterator(s->characters(), s->length());
    if (!it)
        return 0;
    for (int i = 0; i < numGraphemeClusters; ++i)
        if (textBreakNext(it) == TextBreakDone)
            return s->length();
    return textBreakCurrent(it);
}

HTMLInputElement::HTMLInputElement(Document* doc, HTMLFormElement* f)
    : HTMLFormControlElementWithState(inputTag, doc, f)
{
    init();
}

HTMLInputElement::HTMLInputElement(const QualifiedName& tagName, Document* doc, HTMLFormElement* f)
    : HTMLFormControlElementWithState(tagName, doc, f)
{
    init();
}

void HTMLInputElement::init()
{
    m_type = TEXT;
    m_maxLen = cMaxLen;
    m_size = 20;
    m_checked = false;
    m_defaultChecked = false;
    m_useDefaultChecked = true;
    m_indeterminate = false;

    m_haveType = false;
    m_activeSubmit = false;
    m_autocomplete = true;
    m_inited = false;
    m_autofilled = false;

    xPos = 0;
    yPos = 0;
    
    cachedSelStart = -1;
    cachedSelEnd = -1;

    m_maxResults = -1;

    if (form())
        m_autocomplete = form()->autoComplete();
}

HTMLInputElement::~HTMLInputElement()
{
    if (inputType() == PASSWORD)
        document()->unregisterForCacheCallbacks(this);

    document()->checkedRadioButtons().removeButton(this);

    // Need to remove this from the form while it is still an HTMLInputElement,
    // so can't wait for the base class's destructor to do it.
    removeFromForm();
}

const AtomicString& HTMLInputElement::name() const
{
    return m_name.isNull() ? emptyAtom : m_name;
}

static inline HTMLFormElement::CheckedRadioButtons& checkedRadioButtons(const HTMLInputElement *element)
{
    if (HTMLFormElement* form = element->form())
        return form->checkedRadioButtons();
    
    return element->document()->checkedRadioButtons();
}

bool HTMLInputElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    // If text fields can be focused, then they should always be keyboard focusable
    if (isTextField())
        return HTMLFormControlElementWithState::isFocusable();
        
    // If the base class says we can't be focused, then we can stop now.
    if (!HTMLFormControlElementWithState::isKeyboardFocusable(event))
        return false;

    if (inputType() == RADIO) {
        // Unnamed radio buttons are never focusable (matches WinIE).
        if (name().isEmpty())
            return false;

        // Never allow keyboard tabbing to leave you in the same radio group.  Always
        // skip any other elements in the group.
        Node* currentFocusedNode = document()->focusedNode();
        if (currentFocusedNode && currentFocusedNode->hasTagName(inputTag)) {
            HTMLInputElement* focusedInput = static_cast<HTMLInputElement*>(currentFocusedNode);
            if (focusedInput->inputType() == RADIO && focusedInput->form() == form() &&
                focusedInput->name() == name())
                return false;
        }
        
        // Allow keyboard focus if we're checked or if nothing in the group is checked.
        return checked() || !checkedRadioButtons(this).checkedButtonForGroup(name());
    }
    
    return true;
}

bool HTMLInputElement::isMouseFocusable() const
{
    if (isTextField())
        return HTMLFormControlElementWithState::isFocusable();
    return HTMLFormControlElementWithState::isMouseFocusable();
}

void HTMLInputElement::updateFocusAppearance(bool restorePreviousSelection)
{        
    if (isTextField()) {
        if (!restorePreviousSelection || cachedSelStart == -1)
            select();
        else
            // Restore the cached selection.
            setSelectionRange(cachedSelStart, cachedSelEnd); 
        
        if (document() && document()->frame())
            document()->frame()->revealSelection();
    } else
        HTMLFormControlElementWithState::updateFocusAppearance(restorePreviousSelection);
}

void HTMLInputElement::aboutToUnload()
{
    if (isTextField() && focused() && document()->frame())
        document()->frame()->textFieldDidEndEditing(this);
}

bool HTMLInputElement::shouldUseInputMethod() const
{
    return m_type == TEXT || m_type == SEARCH || m_type == ISINDEX;
}

void HTMLInputElement::dispatchFocusEvent()
{
    if (isTextField()) {
        setAutofilled(false);
        if (inputType() == PASSWORD && document()->frame())
            document()->setUseSecureKeyboardEntryWhenActive(true);
    }
    HTMLFormControlElementWithState::dispatchFocusEvent();
}

void HTMLInputElement::dispatchBlurEvent()
{
    if (isTextField() && document()->frame()) {
        if (inputType() == PASSWORD)
            document()->setUseSecureKeyboardEntryWhenActive(false);
        document()->frame()->textFieldDidEndEditing(this);
    }
    HTMLFormControlElementWithState::dispatchBlurEvent();
}

void HTMLInputElement::setType(const String& t)
{
    if (t.isEmpty()) {
        int exccode;
        removeAttribute(typeAttr, exccode);
    } else
        setAttribute(typeAttr, t);
}

void HTMLInputElement::setInputType(const String& t)
{
    InputType newType;
    
    if (equalIgnoringCase(t, "password"))
        newType = PASSWORD;
    else if (equalIgnoringCase(t, "checkbox"))
        newType = CHECKBOX;
    else if (equalIgnoringCase(t, "radio"))
        newType = RADIO;
    else if (equalIgnoringCase(t, "submit"))
        newType = SUBMIT;
    else if (equalIgnoringCase(t, "reset"))
        newType = RESET;
    else if (equalIgnoringCase(t, "file"))
        newType = FILE;
    else if (equalIgnoringCase(t, "hidden"))
        newType = HIDDEN;
    else if (equalIgnoringCase(t, "image"))
        newType = IMAGE;
    else if (equalIgnoringCase(t, "button"))
        newType = BUTTON;
    else if (equalIgnoringCase(t, "khtml_isindex"))
        newType = ISINDEX;
    else if (equalIgnoringCase(t, "search"))
        newType = SEARCH;
    else if (equalIgnoringCase(t, "range"))
        newType = RANGE;
    else
        newType = TEXT;

    // IMPORTANT: Don't allow the type to be changed to FILE after the first
    // type change, otherwise a JavaScript programmer would be able to set a text
    // field's value to something like /etc/passwd and then change it to a file field.
    if (inputType() != newType) {
        if (newType == FILE && m_haveType)
            // Set the attribute back to the old value.
            // Useful in case we were called from inside parseMappedAttribute.
            setAttribute(typeAttr, type());
        else {
            checkedRadioButtons(this).removeButton(this);

            bool wasAttached = m_attached;
            if (wasAttached)
                detach();

            bool didStoreValue = storesValueSeparateFromAttribute();
            bool wasPasswordField = inputType() == PASSWORD;
            bool didRespectHeightAndWidth = respectHeightAndWidthAttrs();
            m_type = newType;
            bool willStoreValue = storesValueSeparateFromAttribute();
            bool isPasswordField = inputType() == PASSWORD;
            bool willRespectHeightAndWidth = respectHeightAndWidthAttrs();

            if (didStoreValue && !willStoreValue && !m_value.isNull()) {
                setAttribute(valueAttr, m_value);
                m_value = String();
            }
            if (!didStoreValue && willStoreValue)
                m_value = constrainValue(getAttribute(valueAttr));
            else
                recheckValue();

            if (wasPasswordField && !isPasswordField)
                document()->unregisterForCacheCallbacks(this);
            else if (!wasPasswordField && isPasswordField)
                document()->registerForCacheCallbacks(this);

            if (didRespectHeightAndWidth != willRespectHeightAndWidth) {
                NamedMappedAttrMap* map = mappedAttributes();
                if (MappedAttribute* height = map->getAttributeItem(heightAttr))
                    attributeChanged(height, false);
                if (MappedAttribute* width = map->getAttributeItem(widthAttr))
                    attributeChanged(width, false);
                if (MappedAttribute* align = map->getAttributeItem(alignAttr))
                    attributeChanged(align, false);
            }

            if (wasAttached)
                attach();

            checkedRadioButtons(this).addButton(this);
        }
    }
    m_haveType = true;

    if (inputType() != IMAGE && m_imageLoader)
        m_imageLoader.clear();
}

const AtomicString& HTMLInputElement::type() const
{
    // needs to be lowercase according to DOM spec
    switch (inputType()) {
        case BUTTON: {
            static const AtomicString button("button");
            return button;
        }
        case CHECKBOX: {
            static const AtomicString checkbox("checkbox");
            return checkbox;
        }
        case FILE: {
            static const AtomicString file("file");
            return file;
        }
        case HIDDEN: {
            static const AtomicString hidden("hidden");
            return hidden;
        }
        case IMAGE: {
            static const AtomicString image("image");
            return image;
        }
        case ISINDEX:
            return emptyAtom;
        case PASSWORD: {
            static const AtomicString password("password");
            return password;
        }
        case RADIO: {
            static const AtomicString radio("radio");
            return radio;
        }
        case RANGE: {
            static const AtomicString range("range");
            return range;
        }
        case RESET: {
            static const AtomicString reset("reset");
            return reset;
        }
        case SEARCH: {
            static const AtomicString search("search");
            return search;
        }
        case SUBMIT: {
            static const AtomicString submit("submit");
            return submit;
        }
        case TEXT: {
            static const AtomicString text("text");
            return text;
        }
    }
    return emptyAtom;
}

bool HTMLInputElement::saveState(String& result) const
{
    switch (inputType()) {
        case BUTTON:
        case FILE:
        case HIDDEN:
        case IMAGE:
        case ISINDEX:
        case RANGE:
        case RESET:
        case SEARCH:
        case SUBMIT:
        case TEXT:
            result = value();
            return true;
        case CHECKBOX:
        case RADIO:
            result = checked() ? "on" : "off";
            return true;
        case PASSWORD:
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void HTMLInputElement::restoreState(const String& state)
{
    ASSERT(inputType() != PASSWORD); // should never save/restore password fields
    switch (inputType()) {
        case BUTTON:
        case FILE:
        case HIDDEN:
        case IMAGE:
        case ISINDEX:
        case RANGE:
        case RESET:
        case SEARCH:
        case SUBMIT:
        case TEXT:
            setValue(state);
            break;
        case CHECKBOX:
        case RADIO:
            setChecked(state == "on");
            break;
        case PASSWORD:
            break;
    }
}

bool HTMLInputElement::canStartSelection() const
{
    if (!isTextField())
        return false;
    return HTMLFormControlElementWithState::canStartSelection();
}

bool HTMLInputElement::canHaveSelection() const
{
    return isTextField();
}

int HTMLInputElement::selectionStart() const
{
    if (!isTextField())
        return 0;
    if (document()->focusedNode() != this && cachedSelStart != -1)
        return cachedSelStart;
    if (!renderer())
        return 0;
    return static_cast<RenderTextControl*>(renderer())->selectionStart();
}

int HTMLInputElement::selectionEnd() const
{
    if (!isTextField())
        return 0;
    if (document()->focusedNode() != this && cachedSelEnd != -1)
        return cachedSelEnd;
    if (!renderer())
        return 0;
    return static_cast<RenderTextControl*>(renderer())->selectionEnd();
}

void HTMLInputElement::setSelectionStart(int start)
{
    if (!isTextField())
        return;
    if (!renderer())
        return;
    static_cast<RenderTextControl*>(renderer())->setSelectionStart(start);
}

void HTMLInputElement::setSelectionEnd(int end)
{
    if (!isTextField())
        return;
    if (!renderer())
        return;
    static_cast<RenderTextControl*>(renderer())->setSelectionEnd(end);
}

void HTMLInputElement::select()
{
    if (!isTextField())
        return;
    if (!renderer())
        return;
    static_cast<RenderTextControl*>(renderer())->select();
}

void HTMLInputElement::setSelectionRange(int start, int end)
{
    if (!isTextField())
        return;
    if (!renderer())
        return;
    static_cast<RenderTextControl*>(renderer())->setSelectionRange(start, end);
}

void HTMLInputElement::accessKeyAction(bool sendToAnyElement)
{
    switch (inputType()) {
        case BUTTON:
        case CHECKBOX:
        case FILE:
        case IMAGE:
        case RADIO:
        case RANGE:
        case RESET:
        case SUBMIT:
            focus(false);
            // send the mouse button events iff the caller specified sendToAnyElement
            dispatchSimulatedClick(0, sendToAnyElement);
            break;
        case HIDDEN:
            // a no-op for this type
            break;
        case ISINDEX:
        case PASSWORD:
        case SEARCH:
        case TEXT:
            // should never restore previous selection here
            focus(false);
            break;
    }
}

bool HTMLInputElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (((attrName == heightAttr || attrName == widthAttr) && respectHeightAndWidthAttrs()) ||
        attrName == vspaceAttr ||
        attrName == hspaceAttr) {
        result = eUniversal;
        return false;
    } 

    if (attrName == alignAttr) {
        if (inputType() == IMAGE) {
            // Share with <img> since the alignment behavior is the same.
            result = eReplaced;
            return false;
        }
    }

    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLInputElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == nameAttr) {
        checkedRadioButtons(this).removeButton(this);
        m_name = attr->value();
        checkedRadioButtons(this).addButton(this);
    } else if (attr->name() == autocompleteAttr) {
        m_autocomplete = !equalIgnoringCase(attr->value(), "off");
    } else if (attr->name() == typeAttr) {
        setInputType(attr->value());
    } else if (attr->name() == valueAttr) {
        // We only need to setChanged if the form is looking at the default value right now.
        if (m_value.isNull())
            setChanged();
        setValueMatchesRenderer(false);
    } else if (attr->name() == checkedAttr) {
        m_defaultChecked = !attr->isNull();
        if (m_useDefaultChecked) {
            setChecked(m_defaultChecked);
            m_useDefaultChecked = true;
        }
    } else if (attr->name() == maxlengthAttr) {
        int oldMaxLen = m_maxLen;
        m_maxLen = !attr->isNull() ? attr->value().toInt() : cMaxLen;
        if (m_maxLen <= 0 || m_maxLen > cMaxLen)
            m_maxLen = cMaxLen;
        if (oldMaxLen != m_maxLen)
            recheckValue();
        setChanged();
    } else if (attr->name() == sizeAttr) {
        m_size = !attr->isNull() ? attr->value().toInt() : 20;
    } else if (attr->name() == altAttr) {
        if (renderer() && inputType() == IMAGE)
            static_cast<RenderImage*>(renderer())->updateAltText();
    } else if (attr->name() == srcAttr) {
        if (renderer() && inputType() == IMAGE) {
            if (!m_imageLoader)
                m_imageLoader.set(new HTMLImageLoader(this));
            m_imageLoader->updateFromElement();
        }
    } else if (attr->name() == usemapAttr ||
               attr->name() == accesskeyAttr) {
        // FIXME: ignore for the moment
    } else if (attr->name() == vspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
    } else if (attr->name() == hspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
    } else if (attr->name() == alignAttr) {
        if (inputType() == IMAGE)
            addHTMLAlignment(attr);
    } else if (attr->name() == widthAttr) {
        if (respectHeightAndWidthAttrs())
            addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    } else if (attr->name() == heightAttr) {
        if (respectHeightAndWidthAttrs())
            addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    } else if (attr->name() == onfocusAttr) {
        setHTMLEventListener(focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setHTMLEventListener(blurEvent, attr);
    } else if (attr->name() == onselectAttr) {
        setHTMLEventListener(selectEvent, attr);
    } else if (attr->name() == onchangeAttr) {
        setHTMLEventListener(changeEvent, attr);
    } else if (attr->name() == oninputAttr) {
        setHTMLEventListener(inputEvent, attr);
    }
    // Search field and slider attributes all just cause updateFromElement to be called through style
    // recalcing.
    else if (attr->name() == onsearchAttr) {
        setHTMLEventListener(searchEvent, attr);
    } else if (attr->name() == resultsAttr) {
        int oldResults = m_maxResults;
        m_maxResults = !attr->isNull() ? min(attr->value().toInt(), maxSavedResults) : -1;
        // FIXME: Detaching just for maxResults change is not ideal.  We should figure out the right
        // time to relayout for this change.
        if (m_maxResults != oldResults && (m_maxResults <= 0 || oldResults <= 0) && attached()) {
            detach();
            attach();
        }
        setChanged();
    } else if (attr->name() == autosaveAttr ||
               attr->name() == incrementalAttr ||
               attr->name() == placeholderAttr ||
               attr->name() == minAttr ||
               attr->name() == maxAttr ||
               attr->name() == precisionAttr) {
        setChanged();
    } else
        HTMLFormControlElementWithState::parseMappedAttribute(attr);
}

bool HTMLInputElement::rendererIsNeeded(RenderStyle *style)
{
    switch (inputType()) {
        case BUTTON:
        case CHECKBOX:
        case FILE:
        case IMAGE:
        case ISINDEX:
        case PASSWORD:
        case RADIO:
        case RANGE:
        case RESET:
        case SEARCH:
        case SUBMIT:
        case TEXT:
            return HTMLFormControlElementWithState::rendererIsNeeded(style);
        case HIDDEN:
            return false;
    }
    ASSERT(false);
    return false;
}

RenderObject *HTMLInputElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    switch (inputType()) {
        case BUTTON:
        case RESET:
        case SUBMIT:
            return new (arena) RenderButton(this);
        case CHECKBOX:
        case RADIO:
            return RenderObject::createObject(this, style);
        case FILE:
            return new (arena) RenderFileUploadControl(this);
        case HIDDEN:
            break;
        case IMAGE:
            return new (arena) RenderImage(this);
        case RANGE:
            return new (arena) RenderSlider(this);
        case ISINDEX:
        case PASSWORD:
        case SEARCH:
        case TEXT:
            return new (arena) RenderTextControl(this, false);             
    }
    ASSERT(false);
    return 0;
}

void HTMLInputElement::attach()
{
    if (!m_inited) {
        if (!m_haveType)
            setInputType(getAttribute(typeAttr));
        m_inited = true;
    }

    HTMLFormControlElementWithState::attach();

    if (inputType() == IMAGE) {
        if (!m_imageLoader)
            m_imageLoader.set(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
        if (renderer()) {
            RenderImage* imageObj = static_cast<RenderImage*>(renderer());
            imageObj->setCachedImage(m_imageLoader->image()); 
            
            // If we have no image at all because we have no src attribute, set
            // image height and width for the alt text instead.
            if (!m_imageLoader->image() && !imageObj->cachedImage())
                imageObj->setImageSizeForAltText();
        }
    }
}

void HTMLInputElement::detach()
{
    HTMLFormControlElementWithState::detach();
    setValueMatchesRenderer(false);
}

String HTMLInputElement::altText() const
{
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    // note this is intentionally different to HTMLImageElement::altText()
    String alt = getAttribute(altAttr);
    // fall back to title attribute
    if (alt.isNull())
        alt = getAttribute(titleAttr);
    if (alt.isNull())
        alt = getAttribute(valueAttr);
    if (alt.isEmpty())
        alt = inputElementAltText();
    return alt;
}

bool HTMLInputElement::isSuccessfulSubmitButton() const
{
    // HTML spec says that buttons must have names to be considered successful.
    // However, other browsers do not impose this constraint. So we do likewise.
    return !disabled() && (inputType() == IMAGE || inputType() == SUBMIT);
}

bool HTMLInputElement::isActivatedSubmit() const
{
    return m_activeSubmit;
}

void HTMLInputElement::setActivatedSubmit(bool flag)
{
    m_activeSubmit = flag;
}

bool HTMLInputElement::appendFormData(FormDataList& encoding, bool multipart)
{
    // image generates its own names, but for other types there is no form data unless there's a name
    if (name().isEmpty() && inputType() != IMAGE)
        return false;

    switch (inputType()) {
        case HIDDEN:
        case ISINDEX:
        case PASSWORD:
        case RANGE:
        case SEARCH:
        case TEXT:
            // always successful
            encoding.appendData(name(), value());
            return true;

        case CHECKBOX:
        case RADIO:
            if (checked()) {
                encoding.appendData(name(), value());
                return true;
            }
            break;

        case BUTTON:
        case RESET:
            // these types of buttons are never successful
            return false;

        case IMAGE:
            if (m_activeSubmit) {
                encoding.appendData(name().isEmpty() ? "x" : (name() + ".x"), xPos);
                encoding.appendData(name().isEmpty() ? "y" : (name() + ".y"), yPos);
                if (!name().isEmpty() && !value().isEmpty())
                    encoding.appendData(name(), value());
                return true;
            }
            break;

        case SUBMIT:
            if (m_activeSubmit) {
                String enc_str = valueWithDefault();
                encoding.appendData(name(), enc_str);
                return true;
            }
            break;

        case FILE:
            // Can't submit file on GET.
            if (!multipart)
                return false;

            // If no filename at all is entered, return successful but empty.
            // Null would be more logical, but Netscape posts an empty file. Argh.
            if (value().isEmpty()) {
                encoding.appendData(name(), String(""));
                return true;
            }

            encoding.appendFile(name(), value());
            return true;
    }
    return false;
}

void HTMLInputElement::reset()
{
    if (storesValueSeparateFromAttribute())
        setValue(String());

    setChecked(m_defaultChecked);
    m_useDefaultChecked = true;
}

void HTMLInputElement::setChecked(bool nowChecked, bool sendChangeEvent)
{
    if (checked() == nowChecked)
        return;

    checkedRadioButtons(this).removeButton(this);

    m_useDefaultChecked = false;
    m_checked = nowChecked;
    setChanged();

    checkedRadioButtons(this).addButton(this);
    
    if (renderer() && renderer()->style()->hasAppearance())
        theme()->stateChanged(renderer(), CheckedState);

    // Only send a change event for items in the document (avoid firing during
    // parsing) and don't send a change event for a radio button that's getting
    // unchecked to match other browsers. DOM is not a useful standard for this
    // because it says only to fire change events at "lose focus" time, which is
    // definitely wrong in practice for these types of elements.
    if (sendChangeEvent && inDocument() && (inputType() != RADIO || nowChecked))
        onChange();
}

void HTMLInputElement::setIndeterminate(bool _indeterminate)
{
    // Only checkboxes honor indeterminate.
    if (inputType() != CHECKBOX || indeterminate() == _indeterminate)
        return;

    m_indeterminate = _indeterminate;

    setChanged();

    if (renderer() && renderer()->style()->hasAppearance())
        theme()->stateChanged(renderer(), CheckedState);
}

void HTMLInputElement::copyNonAttributeProperties(const Element *source)
{
    const HTMLInputElement *sourceElem = static_cast<const HTMLInputElement *>(source);

    m_value = sourceElem->m_value;
    m_checked = sourceElem->m_checked;
    m_indeterminate = sourceElem->m_indeterminate;
    
    HTMLFormControlElementWithState::copyNonAttributeProperties(source);
}

String HTMLInputElement::value() const
{
    String value = m_value;

    // It's important *not* to fall back to the value attribute for file inputs,
    // because that would allow a malicious web page to upload files by setting the
    // value attribute in markup.
    if (value.isNull() && inputType() != FILE)
        value = constrainValue(getAttribute(valueAttr));

    // If no attribute exists, then just use "on" or "" based off the checked() state of the control.
    if (value.isNull() && (inputType() == CHECKBOX || inputType() == RADIO))
        return checked() ? "on" : "";

    return value;
}

String HTMLInputElement::valueWithDefault() const
{
    String v = value();
    if (v.isNull()) {
        switch (inputType()) {
            case BUTTON:
            case CHECKBOX:
            case FILE:
            case HIDDEN:
            case IMAGE:
            case ISINDEX:
            case PASSWORD:
            case RADIO:
            case RANGE:
            case SEARCH:
            case TEXT:
                break;
            case RESET:
                v = resetButtonDefaultLabel();
                break;
            case SUBMIT:
                v = submitButtonDefaultLabel();
                break;
        }
    }
    return v;
}

void HTMLInputElement::setValue(const String& value)
{
    // For security reasons, we don't allow setting the filename, but we do allow clearing it.
    if (inputType() == FILE && !value.isEmpty())
        return;

    setValueMatchesRenderer(false);
    if (storesValueSeparateFromAttribute()) {
        m_value = constrainValue(value);
        if (isTextField() && inDocument())
            document()->updateRendering();
        if (renderer())
            renderer()->updateFromElement();
        setChanged();
    } else
        setAttribute(valueAttr, constrainValue(value));
    
    if (isTextField()) {
        unsigned max = m_value.length();
        if (document()->focusedNode() == this)
            setSelectionRange(max, max);
        else {
            cachedSelStart = max;
            cachedSelEnd = max;
        }
    }
}

void HTMLInputElement::setValueFromRenderer(const String& value)
{
    // Renderer and our event handler are responsible for constraining values.
    ASSERT(value == constrainValue(value) || constrainValue(value).isEmpty());

    // Workaround for bug where trailing \n is included in the result of textContent.
    // The assert macro above may also be simplified to:  value == constrainValue(value)
    // http://bugs.webkit.org/show_bug.cgi?id=9661
    if (value == "\n")
        m_value = "";
    else
        m_value = value;

    setValueMatchesRenderer();

    // Fire the "input" DOM event.
    dispatchHTMLEvent(inputEvent, true, false);
}

bool HTMLInputElement::storesValueSeparateFromAttribute() const
{
    switch (inputType()) {
        case BUTTON:
        case CHECKBOX:
        case HIDDEN:
        case IMAGE:
        case RADIO:
        case RESET:
        case SUBMIT:
            return false;
        case FILE:
        case ISINDEX:
        case PASSWORD:
        case RANGE:
        case SEARCH:
        case TEXT:
            return true;
    }
    return false;
}

void* HTMLInputElement::preDispatchEventHandler(Event *evt)
{
    // preventDefault or "return false" are used to reverse the automatic checking/selection we do here.
    // This result gives us enough info to perform the "undo" in postDispatch of the action we take here.
    void* result = 0; 
    if ((inputType() == CHECKBOX || inputType() == RADIO) && evt->isMouseEvent()
            && evt->type() == clickEvent && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
        if (inputType() == CHECKBOX) {
            // As a way to store the state, we return 0 if we were unchecked, 1 if we were checked, and 2 for
            // indeterminate.
            if (indeterminate()) {
                result = (void*)0x2;
                setIndeterminate(false);
            } else {
                if (checked())
                    result = (void*)0x1;
                setChecked(!checked(), true);
            }
        } else {
            // For radio buttons, store the current selected radio object.
            if (name().isEmpty() || checked())
                return 0; // Match WinIE and don't allow unnamed radio buttons to be checked.
                          // Checked buttons just stay checked.
                          // FIXME: Need to learn to work without a form.

            // We really want radio groups to end up in sane states, i.e., to have something checked.
            // Therefore if nothing is currently selected, we won't allow this action to be "undone", since
            // we want some object in the radio group to actually get selected.
            HTMLInputElement* currRadio = checkedRadioButtons(this).checkedButtonForGroup(name());
            if (currRadio) {
                // We have a radio button selected that is not us.  Cache it in our result field and ref it so
                // that it can't be destroyed.
                currRadio->ref();
                result = currRadio;
            }
            setChecked(true, true);
        }
    }
    return result;
}

void HTMLInputElement::postDispatchEventHandler(Event *evt, void* data)
{
    if ((inputType() == CHECKBOX || inputType() == RADIO) && evt->isMouseEvent()
            && evt->type() == clickEvent && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
        if (inputType() == CHECKBOX) {
            // Reverse the checking we did in preDispatch.
            if (evt->defaultPrevented() || evt->defaultHandled()) {
                if (data == (void*)0x2)
                    setIndeterminate(true);
                else
                    setChecked(data);
            }
        } else if (data) {
            HTMLInputElement* input = static_cast<HTMLInputElement*>(data);
            if (evt->defaultPrevented() || evt->defaultHandled()) {
                // Restore the original selected radio button if possible.
                // Make sure it is still a radio button and only do the restoration if it still
                // belongs to our group.

                // Match WinIE and don't allow unnamed radio buttons to be checked.
                if (input->form() == form() && input->inputType() == RADIO && !name().isEmpty() && input->name() == name()) {
                    // Ok, the old radio button is still in our form and in our group and is still a 
                    // radio button, so it's safe to restore selection to it.
                    input->setChecked(true);
                }
            }
            input->deref();
        }

        // Left clicks on radio buttons and check boxes already performed default actions in preDispatchEventHandler(). 
        evt->setDefaultHandled();
    }
}

void HTMLInputElement::defaultEventHandler(Event* evt)
{
    bool clickDefaultFormButton = false;

    if (isTextField() && evt->type() == textInputEvent && evt->isTextEvent() && static_cast<TextEvent*>(evt)->data() == "\n")
        clickDefaultFormButton = true;

    if (inputType() == IMAGE && evt->isMouseEvent() && evt->type() == clickEvent) {
        // record the mouse position for when we get the DOMActivate event
        MouseEvent* me = static_cast<MouseEvent*>(evt);
        // FIXME: We could just call offsetX() and offsetY() on the event,
        // but that's currently broken, so for now do the computation here.
        if (me->isSimulated() || !renderer()) {
            xPos = 0;
            yPos = 0;
        } else {
            int offsetX, offsetY;
            renderer()->absolutePosition(offsetX, offsetY);
            xPos = me->pageX() - offsetX;
            // FIXME: Why is yPos a short?
            yPos = me->pageY() - offsetY;
        }
    }

    if (isTextField() && evt->type() == keydownEvent && evt->isKeyboardEvent() && focused() && document()->frame()
                && document()->frame()->doTextFieldCommandFromEvent(this, static_cast<KeyboardEvent*>(evt))) {
        evt->setDefaultHandled();
        return;
    }

    if (inputType() == RADIO && evt->isMouseEvent()
            && evt->type() == clickEvent && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
        evt->setDefaultHandled();
        return;
    }
    
    // Let the key handling done in EventTargetNode take precedence over the event handling here for editable text fields
    if (!clickDefaultFormButton) {
        HTMLFormControlElementWithState::defaultEventHandler(evt);
        if (evt->defaultHandled())
            return;
    }

    // DOMActivate events cause the input to be "activated" - in the case of image and submit inputs, this means
    // actually submitting the form. For reset inputs, the form is reset. These events are sent when the user clicks
    // on the element, or presses enter while it is the active element. JavaScript code wishing to activate the element
    // must dispatch a DOMActivate event - a click event will not do the job.
    if (evt->type() == DOMActivateEvent && !disabled()) {
        if (inputType() == IMAGE || inputType() == SUBMIT || inputType() == RESET) {
            if (!form())
                return;
            if (inputType() == RESET)
                form()->reset();
            else {
                m_activeSubmit = true;
                // FIXME: Would be cleaner to get xPos and yPos out of the underlying mouse
                // event (if any) here instead of relying on the variables set above when
                // processing the click event. Even better, appendFormData could pass the
                // event in, and then we could get rid of xPos and yPos altogether!
                if (!form()->prepareSubmit(evt)) {
                    xPos = 0;
                    yPos = 0;
                }
                m_activeSubmit = false;
            }
        } else if (inputType() == FILE && renderer())
            static_cast<RenderFileUploadControl*>(renderer())->click();
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (evt->type() == keypressEvent && evt->isKeyboardEvent()) {
        bool clickElement = false;

        int charCode = static_cast<KeyboardEvent*>(evt)->charCode();

        if (charCode == '\r') {
            switch (inputType()) {
                case CHECKBOX:
                case HIDDEN:
                case ISINDEX:
                case PASSWORD:
                case RANGE:
                case SEARCH:
                case TEXT:
                    // Simulate mouse click on the default form button for enter for these types of elements.
                    clickDefaultFormButton = true;
                    break;
                case BUTTON:
                case FILE:
                case IMAGE:
                case RESET:
                case SUBMIT:
                    // Simulate mouse click for enter for these types of elements.
                    clickElement = true;
                    break;
                case RADIO:
                    break; // Don't do anything for enter on a radio button.
            }
        } else if (charCode == ' ') {
            switch (inputType()) {
                case BUTTON:
                case CHECKBOX:
                case FILE:
                case IMAGE:
                case RESET:
                case SUBMIT:
                case RADIO:
                    // Prevent scrolling down the page.
                    evt->setDefaultHandled();
                    return;
                default:
                    break;
            }
        }

        if (clickElement) {
            dispatchSimulatedClick(evt);
            evt->setDefaultHandled();
            return;
        }
    }

    if (evt->type() == keydownEvent && evt->isKeyboardEvent()) {
        String key = static_cast<KeyboardEvent*>(evt)->keyIdentifier();

        if (key == "U+0020") {
            switch (inputType()) {
                case BUTTON:
                case CHECKBOX:
                case FILE:
                case IMAGE:
                case RESET:
                case SUBMIT:
                case RADIO:
                    setActive(true, true);
                    // No setDefaultHandled() - IE dispatches a keypress in this case.
                    return;
                default:
                    break;
            }
        }

        if (inputType() == RADIO && (key == "Up" || key == "Down" || key == "Left" || key == "Right")) {
            // Left and up mean "previous radio button".
            // Right and down mean "next radio button".
            // Tested in WinIE, and even for RTL, left still means previous radio button (and so moves
            // to the right).  Seems strange, but we'll match it.
            bool forward = (key == "Down" || key == "Right");
            
            // We can only stay within the form's children if the form hasn't been demoted to a leaf because
            // of malformed HTML.
            Node* n = this;
            while ((n = (forward ? n->traverseNextNode() : n->traversePreviousNode()))) {
                // Once we encounter a form element, we know we're through.
                if (n->hasTagName(formTag))
                    break;
                    
                // Look for more radio buttons.
                if (n->hasTagName(inputTag)) {
                    HTMLInputElement* elt = static_cast<HTMLInputElement*>(n);
                    if (elt->form() != form())
                        break;
                    if (n->hasTagName(inputTag)) {
                        // Match WinIE and don't allow unnamed radio buttons to be checked.
                        HTMLInputElement* inputElt = static_cast<HTMLInputElement*>(n);
                        if (inputElt->inputType() == RADIO && !name().isEmpty() && inputElt->name() == name() && inputElt->isFocusable()) {
                            inputElt->setChecked(true);
                            document()->setFocusedNode(inputElt);
                            inputElt->dispatchSimulatedClick(evt, false, false);
                            evt->setDefaultHandled();
                            break;
                        }
                    }
                }
            }
        }
    }

    if (evt->type() == keyupEvent && evt->isKeyboardEvent()) {
        bool clickElement = false;

        String key = static_cast<KeyboardEvent*>(evt)->keyIdentifier();

        if (key == "U+0020") {
            switch (inputType()) {
                case BUTTON:
                case CHECKBOX:
                case FILE:
                case IMAGE:
                case RESET:
                case SUBMIT:
                    // Simulate mouse click for spacebar for these types of elements.
                    // The AppKit already does this for some, but not all, of them.
                    clickElement = true;
                    break;
                case RADIO:
                    // If an unselected radio is tabbed into (because the entire group has nothing
                    // checked, or because of some explicit .focus() call), then allow space to check it.
                    if (!checked())
                        clickElement = true;
                    break;
                case HIDDEN:
                case ISINDEX:
                case PASSWORD:
                case RANGE:
                case SEARCH:
                case TEXT:
                    break;
            }
        }

        if (clickElement) {
            if (active())
                dispatchSimulatedClick(evt);
            evt->setDefaultHandled();
            return;
        }        
    }

    if (clickDefaultFormButton) {
        if (isSearchField()) {
            addSearchResult();
            onSearch();
        }
        // Fire onChange for text fields.
        RenderObject* r = renderer();
        if (r && r->isTextField() && r->isEdited()) {
            onChange();
            // Refetch the renderer since arbitrary JS code run during onchange can do anything, including destroying it.
            r = renderer();
            if (r)
                r->setEdited(false);
        }
        // Form may never have been present, or may have been destroyed by the change event.
        if (form())
            form()->submitClick(evt);
        evt->setDefaultHandled();
        return;
    }

    if (evt->isBeforeTextInsertedEvent()) {
        // Make sure that the text to be inserted will not violate the maxLength.
        int oldLen = numGraphemeClusters(value().impl());
        ASSERT(oldLen <= maxLength());
        int selectionLen = numGraphemeClusters(plainText(document()->frame()->selectionController()->selection().toRange().get()).impl());
        ASSERT(oldLen >= selectionLen);
        int maxNewLen = maxLength() - (oldLen - selectionLen);

        // Truncate the inserted text to avoid violating the maxLength and other constraints.
        BeforeTextInsertedEvent* textEvent = static_cast<BeforeTextInsertedEvent*>(evt);
        textEvent->setText(constrainValue(textEvent->text(), maxNewLen));
    }

    if (isTextField() && renderer() && (evt->isMouseEvent() || evt->isDragEvent() || evt->isWheelEvent() || evt->type() == blurEvent || evt->type() == focusEvent))
        static_cast<RenderTextControl*>(renderer())->forwardEvent(evt);

    if (inputType() == RANGE && renderer()) {
        RenderSlider* slider = static_cast<RenderSlider*>(renderer());
        if (evt->isMouseEvent() && evt->type() == mousedownEvent && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
            MouseEvent* mEvt = static_cast<MouseEvent*>(evt);
            if (!slider->mouseEventIsInThumb(mEvt)) {
                IntPoint eventOffset(mEvt->offsetX(), mEvt->offsetY());
                if (mEvt->target() != this) {
                    IntRect rect = renderer()->absoluteBoundingBoxRect();
                    eventOffset.setX(mEvt->pageX() - rect.x());
                    eventOffset.setY(mEvt->pageY() - rect.y());
                }
                slider->setValueForPosition(slider->positionForOffset(eventOffset));
            }
        }
        if (evt->isMouseEvent() || evt->isDragEvent() || evt->isWheelEvent())
            slider->forwardEvent(evt);
    }
}

bool HTMLInputElement::isURLAttribute(Attribute *attr) const
{
    return (attr->name() == srcAttr);
}

String HTMLInputElement::defaultValue() const
{
    return getAttribute(valueAttr);
}

void HTMLInputElement::setDefaultValue(const String &value)
{
    setAttribute(valueAttr, value);
}

bool HTMLInputElement::defaultChecked() const
{
    return !getAttribute(checkedAttr).isNull();
}

void HTMLInputElement::setDefaultChecked(bool defaultChecked)
{
    setAttribute(checkedAttr, defaultChecked ? "" : 0);
}

String HTMLInputElement::accept() const
{
    return getAttribute(acceptAttr);
}

void HTMLInputElement::setAccept(const String &value)
{
    setAttribute(acceptAttr, value);
}

String HTMLInputElement::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLInputElement::setAccessKey(const String &value)
{
    setAttribute(accesskeyAttr, value);
}

String HTMLInputElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLInputElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

String HTMLInputElement::alt() const
{
    return getAttribute(altAttr);
}

void HTMLInputElement::setAlt(const String &value)
{
    setAttribute(altAttr, value);
}

void HTMLInputElement::setMaxLength(int _maxLength)
{
    setAttribute(maxlengthAttr, String::number(_maxLength));
}

void HTMLInputElement::setSize(unsigned _size)
{
    setAttribute(sizeAttr, String::number(_size));
}

String HTMLInputElement::src() const
{
    return document()->completeURL(getAttribute(srcAttr));
}

void HTMLInputElement::setSrc(const String &value)
{
    setAttribute(srcAttr, value);
}

String HTMLInputElement::useMap() const
{
    return getAttribute(usemapAttr);
}

void HTMLInputElement::setUseMap(const String &value)
{
    setAttribute(usemapAttr, value);
}

String HTMLInputElement::constrainValue(const String& proposedValue) const
{
    return constrainValue(proposedValue, m_maxLen);
}

void HTMLInputElement::recheckValue()
{
    String oldValue = value();
    String newValue = constrainValue(oldValue);
    if (newValue != oldValue)
        setValue(newValue);
}

String HTMLInputElement::constrainValue(const String& proposedValue, int maxLen) const
{
    if (isTextField()) {
        StringImpl* s = proposedValue.impl();
        int newLen = numCharactersInGraphemeClusters(s, maxLen);
        for (int i = 0; i < newLen; ++i) {
            const UChar current = (*s)[i];
            if (current < ' ' && current != '\t') {
                newLen = i;
                break;
            }
        }
        if (newLen < static_cast<int>(proposedValue.length()))
            return proposedValue.substring(0, newLen);
    }
    return proposedValue;
}

void HTMLInputElement::addSearchResult()
{
    ASSERT(isSearchField());
    if (renderer())
        static_cast<RenderTextControl*>(renderer())->addSearchResult();
}

void HTMLInputElement::onSearch()
{
    ASSERT(isSearchField());
    if (renderer())
        static_cast<RenderTextControl*>(renderer())->stopSearchEventTimer();
    dispatchHTMLEvent(searchEvent, true, false);
}

Selection HTMLInputElement::selection() const
{
   if (!renderer() || !isTextField() || cachedSelStart == -1 || cachedSelEnd == -1)
        return Selection();
   return static_cast<RenderTextControl*>(renderer())->selection(cachedSelStart, cachedSelEnd);
}

void HTMLInputElement::didRestoreFromCache()
{
    ASSERT(inputType() == PASSWORD);
    reset();
}

void HTMLInputElement::willMoveToNewOwnerDocument()
{
    if (inputType() == PASSWORD)
        document()->unregisterForCacheCallbacks(this);
        
    document()->checkedRadioButtons().removeButton(this);
    
    HTMLFormControlElementWithState::willMoveToNewOwnerDocument();
}

void HTMLInputElement::didMoveToNewOwnerDocument()
{
    if (inputType() == PASSWORD)
        document()->registerForCacheCallbacks(this);
        
    HTMLFormControlElementWithState::didMoveToNewOwnerDocument();
}
    
} // namespace
