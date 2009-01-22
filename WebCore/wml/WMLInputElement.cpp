/**
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#if ENABLE(WML)
#include "WMLInputElement.h"

#include "Document.h"
#include "EventNames.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "RenderTextControlSingleLine.h"
#include "TextEvent.h"

namespace WebCore {

WMLInputElement::WMLInputElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
    , m_data(this, this)
    , m_isPasswordField(false)
    , m_valueMatchesRenderer(false)
{
}

WMLInputElement::~WMLInputElement()
{
    if (m_isPasswordField)
        document()->unregisterForDocumentActivationCallbacks(this);
}

static inline bool isInputFocusable(RenderObject* renderer)
{
    if (!renderer || !renderer->isBox())
        return false;

    if (RenderBox::toRenderBox(renderer)->size().isEmpty())
        return false;

    if (RenderStyle* style = renderer->style()) {
        if (style->visibility() != VISIBLE)
            return false;
    }

    return true;
}

bool WMLInputElement::isKeyboardFocusable(KeyboardEvent*) const
{
    return isInputFocusable(renderer());
}

bool WMLInputElement::isMouseFocusable() const
{
    return isInputFocusable(renderer());
}

void WMLInputElement::dispatchFocusEvent()
{
    InputElement::dispatchFocusEvent(m_data, document());
    WMLElement::dispatchFocusEvent();
}

void WMLInputElement::dispatchBlurEvent()
{
    InputElement::dispatchBlurEvent(m_data, document());
    WMLElement::dispatchBlurEvent();
}

void WMLInputElement::updateFocusAppearance(bool restorePreviousSelection)
{
    InputElement::updateFocusAppearance(m_data, document(), restorePreviousSelection);
}

void WMLInputElement::aboutToUnload()
{
    InputElement::aboutToUnload(m_data, document());
}

int WMLInputElement::size() const
{
    return m_data.size();
}

const AtomicString& WMLInputElement::name() const
{
    return m_data.name();
}

String WMLInputElement::value() const
{
    String value = m_data.value();
    if (value.isNull())
        value = constrainValue(getAttribute(HTMLNames::valueAttr));

    return value;
}

void WMLInputElement::setValue(const String& value)
{
    InputElement::updatePlaceholderVisibility(m_data, document());
    setValueMatchesRenderer(false);
    m_data.setValue(constrainValue(value));
    if (inDocument())
        document()->updateRendering();
    if (renderer())
        renderer()->updateFromElement();
    setChanged();

    unsigned max = m_data.value().length();
    if (document()->focusedNode() == this)
        InputElement::updateSelectionRange(m_data, max, max);
    else
        cacheSelection(max, max);

    InputElement::notifyFormStateChanged(m_data, document());
}

void WMLInputElement::setValueFromRenderer(const String& value)
{
    InputElement::setValueFromRenderer(m_data, document(), value);
}

bool WMLInputElement::saveState(String& result) const
{
    if (m_isPasswordField)
        return false;

    result = value();
    return true;
}

void WMLInputElement::restoreState(const String& state)
{
    ASSERT(!m_isPasswordField); // should never save/restore password fields
    setValue(state);
}

void WMLInputElement::select()
{
    if (RenderTextControl* r = static_cast<RenderTextControl*>(renderer()))
        r->select();
}

void WMLInputElement::accessKeyAction(bool)
{
    // should never restore previous selection here
    focus(false);
}

void WMLInputElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == HTMLNames::nameAttr)
        m_data.setName(parseValueForbiddingVariableReferences(attr->value()));
    else if (attr->name() == HTMLNames::typeAttr) {
        String type = parseValueForbiddingVariableReferences(attr->value());
        m_isPasswordField = (type == "password");
    } else if (attr->name() == HTMLNames::valueAttr) {
        // We only need to setChanged if the form is looking at the default value right now.
        if (m_data.value().isNull())
            setChanged();
        setValueMatchesRenderer(false);
    } else if (attr->name() == HTMLNames::maxlengthAttr)
        InputElement::parseMaxLengthAttribute(m_data, attr);
    else if (attr->name() == HTMLNames::sizeAttr)
        InputElement::parseSizeAttribute(m_data, attr);
    else
        WMLElement::parseMappedAttribute(attr);

    // FIXME: Handle 'accesskey' attribute
    // FIXME: Handle 'format' attribute
    // FIXME: Handle 'emptyok' attribute
    // FIXME: Handle 'tabindex' attribute
    // FIXME: Handle 'title' attribute
}

void WMLInputElement::copyNonAttributeProperties(const Element* source)
{
    const WMLInputElement* sourceElement = static_cast<const WMLInputElement*>(source);
    m_data.setValue(sourceElement->m_data.value());
    WMLElement::copyNonAttributeProperties(source);
}

RenderObject* WMLInputElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderTextControlSingleLine(this);
}

void WMLInputElement::attach()
{
    ASSERT(!attached());
    WMLElement::attach();

    // The call to updateFromElement() needs to go after the call through
    // to the base class's attach() because that can sometimes do a close
    // on the renderer.
    if (renderer())
        renderer()->updateFromElement();
}  

void WMLInputElement::detach()
{
    WMLElement::detach();
    setValueMatchesRenderer(false);
}
    
bool WMLInputElement::appendFormData(FormDataList& encoding, bool)
{
    if (name().isEmpty())
        return false;

    encoding.appendData(name(), value());
    return true;
}

void WMLInputElement::reset()
{
    setValue(String());
}

void WMLInputElement::defaultEventHandler(Event* evt)
{
    bool clickDefaultFormButton = false;

    if (evt->type() == eventNames().textInputEvent && evt->isTextEvent() && static_cast<TextEvent*>(evt)->data() == "\n")
        clickDefaultFormButton = true;

    if (evt->type() == eventNames().keydownEvent && evt->isKeyboardEvent() && focused() && document()->frame()
        && document()->frame()->doTextFieldCommandFromEvent(this, static_cast<KeyboardEvent*>(evt))) {
        evt->setDefaultHandled();
        return;
    }
    
    // Let the key handling done in EventTargetNode take precedence over the event handling here for editable text fields
    if (!clickDefaultFormButton) {
        WMLElement::defaultEventHandler(evt);
        if (evt->defaultHandled())
            return;
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (evt->type() == eventNames().keypressEvent && evt->isKeyboardEvent()) {
        // Simulate mouse click on the default form button for enter for these types of elements.
        if (static_cast<KeyboardEvent*>(evt)->charCode() == '\r')
            clickDefaultFormButton = true;
    }

    if (clickDefaultFormButton) {
        // Fire onChange for text fields.
        RenderObject* r = renderer();
        if (r && r->isEdited()) {
            dispatchEventForType(eventNames().changeEvent, true, false);
            
            // Refetch the renderer since arbitrary JS code run during onchange can do anything, including destroying it.
            r = renderer();
            if (r)
                r->setEdited(false);
        }

        evt->setDefaultHandled();
        return;
    }

    if (evt->isBeforeTextInsertedEvent())
        InputElement::handleBeforeTextInsertedEvent(m_data, document(), evt);

    if (renderer() && (evt->isMouseEvent() || evt->isDragEvent() || evt->isWheelEvent() || evt->type() == eventNames().blurEvent || evt->type() == eventNames().focusEvent))
        static_cast<RenderTextControlSingleLine*>(renderer())->forwardEvent(evt);
}

void WMLInputElement::cacheSelection(int start, int end)
{
    m_data.setCachedSelectionStart(start);
    m_data.setCachedSelectionEnd(end);
}

String WMLInputElement::constrainValue(const String& proposedValue) const
{
    return InputElement::constrainValue(m_data, proposedValue, m_data.maxLength());
}

void WMLInputElement::documentDidBecomeActive()
{
    ASSERT(m_isPasswordField);
    reset();
}

bool WMLInputElement::placeholderShouldBeVisible() const
{
    return m_data.placeholderShouldBeVisible();
}

void WMLInputElement::willMoveToNewOwnerDocument()
{
    // Always unregister for cache callbacks when leaving a document, even if we would otherwise like to be registered
    if (m_isPasswordField)
        document()->unregisterForDocumentActivationCallbacks(this);

    WMLElement::willMoveToNewOwnerDocument();
}

void WMLInputElement::didMoveToNewOwnerDocument()
{
    if (m_isPasswordField)
        document()->registerForDocumentActivationCallbacks(this);

    WMLElement::didMoveToNewOwnerDocument();
}

}

#endif
