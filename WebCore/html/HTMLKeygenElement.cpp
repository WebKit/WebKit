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
#include "HTMLKeygenElement.h"
#include "HTMLOptionElement.h"
#include "Text.h"
#include "Document.h"
#include "FormDataList.h"

#include <ksslkeygen.h>

using namespace WebCore;

namespace WebCore {

using namespace HTMLNames;

HTMLKeygenElement::HTMLKeygenElement(Document* doc, HTMLFormElement* f)
    : HTMLSelectElement(keygenTag, doc, f)
{
    DeprecatedStringList keys = KSSLKeyGen::supportedKeySizes();
    for (DeprecatedStringList::Iterator i = keys.begin(); i != keys.end(); ++i) {
        HTMLOptionElement* o = new HTMLOptionElement(doc, form());
        addChild(o);
        o->addChild(new Text(doc, String(*i)));
    }
}

String HTMLKeygenElement::type() const
{
    return "keygen";
}

void HTMLKeygenElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == challengeAttr)
        m_challenge = attr->value();
    else if (attr->name() == keytypeAttr)
        m_keyType = attr->value();
    else
        // skip HTMLSelectElement parsing!
        HTMLGenericFormElement::parseMappedAttribute(attr);
}

bool HTMLKeygenElement::appendFormData(FormDataList& encoded_values, bool)
{
    // Only RSA is supported at this time.
    if (!m_keyType.isNull() && !equalIgnoringCase(m_keyType, "rsa"))
        return false;
    DeprecatedString value = KSSLKeyGen::signedPublicKeyAndChallengeString(selectedIndex(), m_challenge.deprecatedString(), document()->baseURL());
    if (value.isNull())
        return false;
    encoded_values.appendData(name(), value.utf8());
    return true;
}

} // namespace
