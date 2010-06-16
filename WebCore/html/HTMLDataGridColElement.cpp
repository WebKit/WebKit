/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(DATAGRID)

#include "Attribute.h"
#include "DataGridColumn.h"
#include "HTMLDataGridColElement.h"
#include "HTMLDataGridElement.h"
#include "HTMLNames.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLDataGridColElement::HTMLDataGridColElement(const QualifiedName& name, Document* document)
    : HTMLElement(name, document)
    , m_dataGrid(0)
{
}

PassRefPtr<HTMLDataGridColElement> HTMLDataGridColElement::create(const QualifiedName& name, Document* document)
{
    return adoptRef(new HTMLDataGridColElement(name, document));
}

HTMLDataGridElement* HTMLDataGridColElement::findDataGridAncestor() const
{
    if (parent() && parent()->hasTagName(datagridTag))
        return static_cast<HTMLDataGridElement*>(parent());
    return 0;
}

void HTMLDataGridColElement::ensureColumn()
{
    if (m_column)
        return;
    m_column = DataGridColumn::create(getIdAttribute(), label(), type(), primary(), sortable());
}

void HTMLDataGridColElement::insertedIntoTree(bool deep)
{
    HTMLElement::insertedIntoTree(deep);
    if (dataGrid()) // We're connected to a datagrid already.
        return;
    m_dataGrid = findDataGridAncestor();
    if (dataGrid() && dataGrid()->dataSource()->isDOMDataGridDataSource()) {
        ensureColumn();
        m_dataGrid->columns()->add(column()); // FIXME: Deal with ordering issues (complicated, since columns can be made outside the DOM).
    }
}

void HTMLDataGridColElement::removedFromTree(bool deep)
{
    HTMLElement::removedFromTree(deep);
    if (dataGrid() && dataGrid()->dataSource()->isDOMDataGridDataSource()) {
        HTMLDataGridElement* grid = findDataGridAncestor();
        if (!grid && column()) {
            dataGrid()->columns()->remove(column());
            m_dataGrid = 0;
        }
    }
}
    
String HTMLDataGridColElement::label() const
{
    return getAttribute(labelAttr);
}

void HTMLDataGridColElement::setLabel(const String& label)
{
    setAttribute(labelAttr, label);
}

String HTMLDataGridColElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLDataGridColElement::setType(const String& type)
{
    setAttribute(typeAttr, type);
}

unsigned short HTMLDataGridColElement::sortable() const
{
    if (!hasAttribute(sortableAttr))
        return 2;
    return getAttribute(sortableAttr).toInt(0);
}

void HTMLDataGridColElement::setSortable(unsigned short sortable)
{
    setAttribute(sortableAttr, String::number(sortable));
}

unsigned short HTMLDataGridColElement::sortDirection() const
{
    String sortDirection = getAttribute(sortdirectionAttr);
    if (equalIgnoringCase(sortDirection, "ascending"))
        return 1;
    if (equalIgnoringCase(sortDirection, "descending"))
        return 2;
    return 0;
}

void HTMLDataGridColElement::setSortDirection(unsigned short sortDirection)
{
    // FIXME: Check sortable rules.
    if (sortDirection == 0)
        setAttribute(sortdirectionAttr, "natural");
    else if (sortDirection == 1)
        setAttribute(sortdirectionAttr, "ascending");
    else if (sortDirection == 2)
        setAttribute(sortdirectionAttr, "descending");
}

bool HTMLDataGridColElement::primary() const
{
    return hasAttribute(primaryAttr);
}

void HTMLDataGridColElement::setPrimary(bool primary)
{
    setAttribute(primaryAttr, primary ? "" : 0);
}

void HTMLDataGridColElement::parseMappedAttribute(Attribute* attr) 
{
    HTMLElement::parseMappedAttribute(attr);
     
    if (!column())
        return;

    if (attr->name() == labelAttr)
        column()->setLabel(label());
    else if (attr->name() == typeAttr)
        column()->setType(type());
    else if (attr->name() == primaryAttr)
        column()->setPrimary(primary());
    else if (attr->name() == sortableAttr)
        column()->setSortable(sortable());
    else if (attr->name() == sortdirectionAttr)
        column()->setSortDirection(sortDirection());
    else if (isIdAttributeName(attr->name()))
        column()->setId(getIdAttribute());
}

} // namespace WebCore

#endif
