/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 
#include "config.h"
#include "HTMLSelectElementImpl.h"

#include "DocumentImpl.h"
#include "EventNames.h"
#include "FormDataList.h"
#include "HTMLCollectionImpl.h"
#include "HTMLFormElementImpl.h"
#include "HTMLOptionElementImpl.h"
#include "HTMLOptionsCollectionImpl.h"
#include "cssproperties.h"
#include "cssstyleselector.h"
#include "dom2_eventsimpl.h"
#include "render_form.h"
#include <assert.h>

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLSelectElementImpl::HTMLSelectElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(selectTag, doc, f), m_minwidth(0), m_size(0), m_multiple(false), m_recalcListItems(false)
{
}

HTMLSelectElementImpl::HTMLSelectElementImpl(const QualifiedName& tagName, DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(tagName, doc, f)
{
}

HTMLSelectElementImpl::~HTMLSelectElementImpl()
{
    if (getDocument())
        getDocument()->deregisterMaintainsState(this);
    if (m_options)
        m_options->detach();
}

bool HTMLSelectElementImpl::checkDTD(const NodeImpl* newChild)
{
    return newChild->isTextNode() || newChild->hasTagName(optionTag) || newChild->hasTagName(optgroupTag) || newChild->hasTagName(hrTag) ||
           newChild->hasTagName(scriptTag);
}

void HTMLSelectElementImpl::recalcStyle( StyleChange ch )
{
    if (hasChangedChild() && m_render)
        static_cast<RenderSelect*>(m_render)->setOptionsChanged(true);

    HTMLGenericFormElementImpl::recalcStyle( ch );
}


DOMString HTMLSelectElementImpl::type() const
{
    return (m_multiple ? "select-multiple" : "select-one");
}

int HTMLSelectElementImpl::selectedIndex() const
{
    // return the number of the first option selected
    uint o = 0;
    Array<HTMLElementImpl*> items = listItems();
    for (unsigned int i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            if (static_cast<HTMLOptionElementImpl*>(items[i])->selected())
                return o;
            o++;
        }
    }
    Q_ASSERT(m_multiple);
    return -1;
}

void HTMLSelectElementImpl::setSelectedIndex( int  index )
{
    // deselect all other options and select only the new one
    Array<HTMLElementImpl*> items = listItems();
    int listIndex;
    for (listIndex = 0; listIndex < int(items.size()); listIndex++) {
        if (items[listIndex]->hasLocalName(optionTag))
            static_cast<HTMLOptionElementImpl*>(items[listIndex])->setSelected(false);
    }
    listIndex = optionToListIndex(index);
    if (listIndex >= 0)
        static_cast<HTMLOptionElementImpl*>(items[listIndex])->setSelected(true);

    setChanged(true);
}

int HTMLSelectElementImpl::length() const
{
    int len = 0;
    uint i;
    Array<HTMLElementImpl*> items = listItems();
    for (i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag))
            len++;
    }
    return len;
}

void HTMLSelectElementImpl::add( HTMLElementImpl *element, HTMLElementImpl *before, int &exceptioncode )
{
    RefPtr<HTMLElementImpl> protectNewChild(element); // make sure the element is ref'd and deref'd so we don't leak it

    if (!element || !(element->hasLocalName(optionTag) || element->hasLocalName(hrTag)))
        return;

    insertBefore(element, before, exceptioncode);
    if (!exceptioncode)
        setRecalcListItems();
}

void HTMLSelectElementImpl::remove( int index )
{
    int exceptioncode = 0;
    int listIndex = optionToListIndex(index);

    Array<HTMLElementImpl*> items = listItems();
    if(listIndex < 0 || index >= int(items.size()))
        return; // ### what should we do ? remove the last item?

    NodeImpl *item = items[listIndex];
    item->ref();
    removeChild(item, exceptioncode);
    item->deref();
    if( !exceptioncode )
        setRecalcListItems();
}

DOMString HTMLSelectElementImpl::value()
{
    uint i;
    Array<HTMLElementImpl*> items = listItems();
    for (i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag) && static_cast<HTMLOptionElementImpl*>(items[i])->selected())
            return static_cast<HTMLOptionElementImpl*>(items[i])->value();
    }
    return DOMString("");
}

void HTMLSelectElementImpl::setValue(const DOMString &value)
{
    if (value.isNull())
        return;
    // find the option with value() matching the given parameter
    // and make it the current selection.
    Array<HTMLElementImpl*> items = listItems();
    for (unsigned i = 0; i < items.size(); i++)
        if (items[i]->hasLocalName(optionTag) && static_cast<HTMLOptionElementImpl*>(items[i])->value() == value) {
            static_cast<HTMLOptionElementImpl*>(items[i])->setSelected(true);
            return;
        }
}

QString HTMLSelectElementImpl::state()
{
    Array<HTMLElementImpl*> items = listItems();

    int l = items.count();
    QString state;
    for(int i = 0; i < l; i++)
        if(items[i]->hasLocalName(optionTag) && static_cast<HTMLOptionElementImpl*>(items[i])->selected())
            state += 'X';
        else
            state += '.';

    return HTMLGenericFormElementImpl::state() + state;
}

void HTMLSelectElementImpl::restoreState(QStringList &_states)
{
    QString _state = HTMLGenericFormElementImpl::findMatchingState(_states);
    if (_state.isNull()) return;

    recalcListItems();

    QString state = _state;
    if(!state.isEmpty() && !state.contains('X') && !m_multiple) {
        qWarning("should not happen in restoreState!");
        // KWQString doesn't support this operation. Should never get here anyway.
        //state[0] = 'X';
    }

    Array<HTMLElementImpl*> items = listItems();

    int l = items.count();
    for (int i = 0; i < l; i++) {
        if (items[i]->hasLocalName(optionTag)) {
            HTMLOptionElementImpl* oe = static_cast<HTMLOptionElementImpl*>(items[i]);
            oe->setSelected(state[i] == 'X');
        }
    }
    setChanged(true);
}

PassRefPtr<NodeImpl> HTMLSelectElementImpl::insertBefore(PassRefPtr<NodeImpl> newChild, NodeImpl* refChild, ExceptionCode& ec)
{
    PassRefPtr<NodeImpl> result = HTMLGenericFormElementImpl::insertBefore(newChild, refChild, ec);
    if (result)
        setRecalcListItems();
    return result;
}

PassRefPtr<NodeImpl> HTMLSelectElementImpl::replaceChild(PassRefPtr<NodeImpl> newChild, NodeImpl *oldChild, ExceptionCode& ec)
{
    PassRefPtr<NodeImpl> result = HTMLGenericFormElementImpl::replaceChild(newChild, oldChild, ec);
    if (result)
        setRecalcListItems();
    return result;
}

PassRefPtr<NodeImpl> HTMLSelectElementImpl::removeChild(NodeImpl* oldChild, ExceptionCode& ec)
{
    PassRefPtr<NodeImpl> result = HTMLGenericFormElementImpl::removeChild(oldChild, ec);
    if (result)
        setRecalcListItems();
    return result;
}

PassRefPtr<NodeImpl> HTMLSelectElementImpl::appendChild(PassRefPtr<NodeImpl> newChild, ExceptionCode& ec)
{
    PassRefPtr<NodeImpl> result = HTMLGenericFormElementImpl::appendChild(newChild, ec);
    if (result)
        setRecalcListItems();
    return result;
}

ContainerNodeImpl* HTMLSelectElementImpl::addChild(PassRefPtr<NodeImpl> newChild)
{
    ContainerNodeImpl* result = HTMLGenericFormElementImpl::addChild(newChild);
    if (result)
        setRecalcListItems();
    return result;
}

void HTMLSelectElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == sizeAttr) {
        m_size = kMax(attr->value().toInt(), 1);
    } else if (attr->name() == widthAttr) {
        m_minwidth = kMax( attr->value().toInt(), 0 );
    } else if (attr->name() == multipleAttr) {
        m_multiple = (!attr->isNull());
    } else if (attr->name() == accesskeyAttr) {
        // FIXME: ignore for the moment
    } else if (attr->name() == onfocusAttr) {
        setHTMLEventListener(focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setHTMLEventListener(blurEvent, attr);
    } else if (attr->name() == onchangeAttr) {
        setHTMLEventListener(changeEvent, attr);
    } else
        HTMLGenericFormElementImpl::parseMappedAttribute(attr);
}

RenderObject *HTMLSelectElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderSelect(this);
}

bool HTMLSelectElementImpl::appendFormData(FormDataList& list, bool)
{
    bool successful = false;
    Array<HTMLElementImpl*> items = listItems();

    uint i;
    for (i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            HTMLOptionElementImpl *option = static_cast<HTMLOptionElementImpl*>(items[i]);
            if (option->selected()) {
                list.appendData(name(), option->value());
                successful = true;
            }
        }
    }

    // ### this case should not happen. make sure that we select the first option
    // in any case. otherwise we have no consistency with the DOM interface. FIXME!
    // we return the first one if it was a combobox select
    if (!successful && !m_multiple && m_size <= 1 && items.size() &&
        (items[0]->hasLocalName(optionTag))) {
        HTMLOptionElementImpl *option = static_cast<HTMLOptionElementImpl*>(items[0]);
        if (option->value().isNull())
            list.appendData(name(), option->text().qstring().stripWhiteSpace());
        else
            list.appendData(name(), option->value());
        successful = true;
    }

    return successful;
}

int HTMLSelectElementImpl::optionToListIndex(int optionIndex) const
{
    Array<HTMLElementImpl*> items = listItems();
    if (optionIndex < 0 || optionIndex >= int(items.size()))
        return -1;

    int listIndex = 0;
    int optionIndex2 = 0;
    for (;
         optionIndex2 < int(items.size()) && optionIndex2 <= optionIndex;
         listIndex++) { // not a typo!
        if (items[listIndex]->hasLocalName(optionTag))
            optionIndex2++;
    }
    listIndex--;
    return listIndex;
}

int HTMLSelectElementImpl::listToOptionIndex(int listIndex) const
{
    Array<HTMLElementImpl*> items = listItems();
    if (listIndex < 0 || listIndex >= int(items.size()) ||
        !items[listIndex]->hasLocalName(optionTag))
        return -1;

    int optionIndex = 0; // actual index of option not counting OPTGROUP entries that may be in list
    int i;
    for (i = 0; i < listIndex; i++)
        if (items[i]->hasLocalName(optionTag))
            optionIndex++;
    return optionIndex;
}

// FIXME 4197997: This method is used by the public Objective-C DOM API -[DOMHTMLSelectElement options],
// but always returns an empty list.
HTMLOptionsCollectionImpl* HTMLSelectElementImpl::options()
{
    if (!m_options)
        m_options = new HTMLOptionsCollectionImpl(this);
    return m_options.get();
}

// FIXME: Delete this once the above function is working well enough to use for real.
RefPtr<HTMLCollectionImpl> HTMLSelectElementImpl::optionsHTMLCollection()
{
    return RefPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::SELECT_OPTIONS));
}

void HTMLSelectElementImpl::recalcListItems()
{
    NodeImpl* current = firstChild();
    m_listItems.resize(0);
    HTMLOptionElementImpl* foundSelected = 0;
    while(current) {
        if (current->hasTagName(optgroupTag) && current->firstChild()) {
            // ### what if optgroup contains just comments? don't want one of no options in it...
            m_listItems.resize(m_listItems.size()+1);
            m_listItems[m_listItems.size()-1] = static_cast<HTMLElementImpl*>(current);
            current = current->firstChild();
        }
        if (current->hasTagName(optionTag)) {
            m_listItems.resize(m_listItems.size()+1);
            m_listItems[m_listItems.size()-1] = static_cast<HTMLElementImpl*>(current);
            if (!foundSelected && !m_multiple && m_size <= 1) {
                foundSelected = static_cast<HTMLOptionElementImpl*>(current);
                foundSelected->m_selected = true;
            }
            else if (foundSelected && !m_multiple && static_cast<HTMLOptionElementImpl*>(current)->selected()) {
                foundSelected->m_selected = false;
                foundSelected = static_cast<HTMLOptionElementImpl*>(current);
            }
        }
        if (current->hasTagName(hrTag)) {
            m_listItems.resize(m_listItems.size()+1);
            m_listItems[m_listItems.size()-1] = static_cast<HTMLElementImpl*>(current);
        }
        NodeImpl *parent = current->parentNode();
        current = current->nextSibling();
        if (!current) {
            if (parent != this)
                current = parent->nextSibling();
        }
    }
    m_recalcListItems = false;
}

void HTMLSelectElementImpl::childrenChanged()
{
    setRecalcListItems();

    HTMLGenericFormElementImpl::childrenChanged();
}

void HTMLSelectElementImpl::setRecalcListItems()
{
    m_recalcListItems = true;
    if (m_render)
        static_cast<RenderSelect*>(m_render)->setOptionsChanged(true);
    setChanged();
}

void HTMLSelectElementImpl::reset()
{
    Array<HTMLElementImpl*> items = listItems();
    uint i;
    for (i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            HTMLOptionElementImpl *option = static_cast<HTMLOptionElementImpl*>(items[i]);
            bool selected = (!option->getAttribute(selectedAttr).isNull());
            option->setSelected(selected);
        }
    }
    if ( m_render )
        static_cast<RenderSelect*>(m_render)->setSelectionChanged(true);
    setChanged( true );
}

void HTMLSelectElementImpl::notifyOptionSelected(HTMLOptionElementImpl *selectedOption, bool selected)
{
    if (selected && !m_multiple) {
        // deselect all other options
        Array<HTMLElementImpl*> items = listItems();
        uint i;
        for (i = 0; i < items.size(); i++) {
            if (items[i]->hasLocalName(optionTag))
                static_cast<HTMLOptionElementImpl*>(items[i])->m_selected = (items[i] == selectedOption);
        }
    }
    if (m_render)
        static_cast<RenderSelect*>(m_render)->setSelectionChanged(true);

    setChanged(true);
}

void HTMLSelectElementImpl::defaultEventHandler(EventImpl *evt)
{
    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (evt->type() == keypressEvent) {
    
        if (!m_form || !m_render || !evt->isKeyboardEvent())
            return;
        
        DOMString key = static_cast<KeyboardEventImpl *>(evt)->keyIdentifier();
        
        if (key == "Enter") {
            m_form->submitClick();
            evt->setDefaultHandled();
        }
    }
    HTMLGenericFormElementImpl::defaultEventHandler(evt);
}

void HTMLSelectElementImpl::accessKeyAction(bool)
{
    focus();
}

void HTMLSelectElementImpl::setMultiple(bool multiple)
{
    setAttribute(multipleAttr, multiple ? "" : 0);
}

void HTMLSelectElementImpl::setSize(int size)
{
    setAttribute(sizeAttr, QString::number(size));
}
    
} // namespace
