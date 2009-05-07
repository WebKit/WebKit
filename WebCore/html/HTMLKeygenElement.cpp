/*
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "HTMLKeygenElement.h"

#include "Document.h"
#include "FormDataList.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "MappedAttribute.h"
#include "SSLKeyGenerator.h"
#include "Text.h"
#include <wtf/StdLibExtras.h>

using namespace WebCore;

namespace WebCore {

using namespace HTMLNames;

HTMLKeygenElement::HTMLKeygenElement(const QualifiedName& tagName, Document* doc, HTMLFormElement* f)
    : HTMLSelectElement(tagName, doc, f)
{
    ASSERT(hasTagName(keygenTag));
    Vector<String> keys;
    getSupportedKeySizes(keys);
        
    Vector<String>::const_iterator end = keys.end();
    for (Vector<String>::const_iterator it = keys.begin(); it != end; ++it) {
        HTMLOptionElement* o = new HTMLOptionElement(optionTag, doc, form());
        addChild(o);
        o->addChild(new Text(doc, *it));
    }
}

const AtomicString& HTMLKeygenElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, keygen, ("keygen"));
    return keygen;
}

void HTMLKeygenElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == challengeAttr)
        m_challenge = attr->value();
    else if (attr->name() == keytypeAttr)
        m_keyType = attr->value();
    else
        // skip HTMLSelectElement parsing!
        HTMLFormControlElement::parseMappedAttribute(attr);
}

bool HTMLKeygenElement::appendFormData(FormDataList& encoded_values, bool)
{
    // Only RSA is supported at this time.
    if (!m_keyType.isNull() && !equalIgnoringCase(m_keyType, "rsa"))
        return false;
    String value = signedPublicKeyAndChallengeString(selectedIndex(), m_challenge, document()->baseURL());
    if (value.isNull())
        return false;
    encoded_values.appendData(name(), value.utf8());
    return true;
}

} // namespace
