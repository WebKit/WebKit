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
#include "HTMLKeygenElementImpl.h"
#include "HTMLOptionElementImpl.h"
#include "dom_textimpl.h"
#include "dom_docimpl.h"
#include "FormDataList.h"

#include <ksslkeygen.h>

using namespace khtml;

namespace DOM {

using namespace HTMLNames;

HTMLKeygenElementImpl::HTMLKeygenElementImpl(DocumentImpl* doc, HTMLFormElementImpl* f)
    : HTMLSelectElementImpl(keygenTag, doc, f)
{
    QStringList keys = KSSLKeyGen::supportedKeySizes();
    for (QStringList::Iterator i = keys.begin(); i != keys.end(); ++i) {
        HTMLOptionElementImpl* o = new HTMLOptionElementImpl(doc, form());
        addChild(o);
        o->addChild(new TextImpl(doc, DOMString(*i)));
    }
}

DOMString HTMLKeygenElementImpl::type() const
{
    return "keygen";
}

void HTMLKeygenElementImpl::parseMappedAttribute(MappedAttributeImpl* attr)
{
    if (attr->name() == challengeAttr)
        m_challenge = attr->value();
    else if (attr->name() == keytypeAttr)
        m_keyType = attr->value();
    else
        // skip HTMLSelectElementImpl parsing!
        HTMLGenericFormElementImpl::parseMappedAttribute(attr);
}

bool HTMLKeygenElementImpl::appendFormData(FormDataList& encoded_values, bool)
{
    // Only RSA is supported at this time.
    if (!m_keyType.isNull() && !equalIgnoringCase(m_keyType, "rsa"))
        return false;
    QString value = KSSLKeyGen::signedPublicKeyAndChallengeString(selectedIndex(), m_challenge.qstring(), getDocument()->baseURL());
    if (value.isNull())
        return false;
    encoded_values.appendData(name(), value.utf8());
    return true;
}

} // namespace
