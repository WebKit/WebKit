/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Alonzo
 * Copyright (C) 2012 Igalia S.L.
 *
 * Portions from Mozilla a11y, copyright as follows:
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
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
 */

#include "config.h"
#include "WebKitAccessibleInterfaceDocument.h"

#include "AccessibilityObject.h"
#include "Document.h"
#include "DocumentType.h"
#include "WebKitAccessibleUtil.h"
#include "WebKitAccessibleWrapperAtk.h"

using namespace WebCore;

static AccessibilityObject* core(AtkDocument* document)
{
    if (!WEBKIT_IS_ACCESSIBLE(document))
        return 0;

    return webkitAccessibleGetAccessibilityObject(WEBKIT_ACCESSIBLE(document));
}

static const gchar* documentAttributeValue(AtkDocument* document, const gchar* attribute)
{
    Document* coreDocument = core(document)->document();
    if (!coreDocument)
        return 0;

    String value = String();
    if (!g_ascii_strcasecmp(attribute, "DocType") && coreDocument->doctype())
        value = coreDocument->doctype()->name();
    else if (!g_ascii_strcasecmp(attribute, "Encoding"))
        value = coreDocument->charset();
    else if (!g_ascii_strcasecmp(attribute, "URI"))
        value = coreDocument->documentURI();

    if (!value.isEmpty())
        return returnString(value);

    return 0;
}

static const gchar* webkitAccessibleDocumentGetAttributeValue(AtkDocument* document, const gchar* attribute)
{
    return documentAttributeValue(document, attribute);
}

static AtkAttributeSet* webkitAccessibleDocumentGetAttributes(AtkDocument* document)
{
    AtkAttributeSet* attributeSet = 0;
    const gchar* attributes[] = { "DocType", "Encoding", "URI" };

    for (unsigned i = 0; i < G_N_ELEMENTS(attributes); i++) {
        const gchar* value = documentAttributeValue(document, attributes[i]);
        if (value)
            attributeSet = addToAtkAttributeSet(attributeSet, attributes[i], value);
    }

    return attributeSet;
}

static const gchar* webkitAccessibleDocumentGetLocale(AtkDocument* document)
{
    // TODO: Should we fall back on lang xml:lang when the following comes up empty?
    String language = core(document)->language();
    if (!language.isEmpty())
        return returnString(language);

    return 0;
}

void webkitAccessibleDocumentInterfaceInit(AtkDocumentIface* iface)
{
    iface->get_document_attribute_value = webkitAccessibleDocumentGetAttributeValue;
    iface->get_document_attributes = webkitAccessibleDocumentGetAttributes;
    iface->get_document_locale = webkitAccessibleDocumentGetLocale;
}
