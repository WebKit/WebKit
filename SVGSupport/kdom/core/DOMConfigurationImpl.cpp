/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "kdom.h"
#include "DOMStringImpl.h"
#include "DOMUserDataImpl.h"
#include "DOMStringListImpl.h"
#include "DOMExceptionImpl.h"
#include "DOMConfigurationImpl.h"

using namespace KDOM;

DOMStringListImpl *DOMConfigurationImpl::m_paramNames = 0;

DOMConfigurationImpl::DOMConfigurationImpl() : Shared()
{
    m_flags = 0;
    m_flags |= FEATURE_COMMENTS;
    m_flags |= FEATURE_CDATA_SECTIONS;
    m_flags |= FEATURE_WHITESPACE_IN_ELEMENT_CONTENT;
    m_flags |= FEATURE_ENTITIES;
    m_flags |= FEATURE_NAMESPACES;
    m_flags |= FEATURE_NAMESPACE_DECLARATIONS;
    m_flags |= FEATURE_SPLIT_CDATA_SECTIONS;
    m_flags |= FEATURE_WELL_FORMED;
    m_flags |= FEATURE_CHARSET_OVERRIDES_XML_ENCODING;
    m_flags |= FEATURE_DISCARD_DEFAULT_CONTENT;
    m_flags |= FEATURE_XML_DECLARATION;
    m_flags |= FEATURE_IGNORE_UNKNOWN_CD;

    m_errHandler = 0;
}

DOMConfigurationImpl::~DOMConfigurationImpl()
{
    if(m_errHandler)
        m_errHandler->deref();
}

void DOMConfigurationImpl::setParameter(DOMStringImpl *name, DOMUserDataImpl *value)
{
    if(!value || !name)
        return;

    DOMString temp(name);
    if(temp.lower() == COMMENTS)
    {
        bool b = (bool) value->userData();
        kdDebug() << "b : " << b << endl;
        m_flags |= (b ? FEATURE_COMMENTS : 0);
    }
}

void DOMConfigurationImpl::setParameter(DOMStringImpl *name, bool value)
{
    if(!name)
        return;

    DOMString temp(name);
    if(temp.lower() == COMMENTS)
    {
        if(value)
            m_flags |= FEATURE_COMMENTS;
        else
            m_flags &= ~FEATURE_COMMENTS;
    }
    else if(temp.lower() == CDATA_SECTIONS)
    {
        if(value)
            m_flags |= FEATURE_CDATA_SECTIONS;
        else
            m_flags &= ~FEATURE_CDATA_SECTIONS;
    }
    else if(temp.lower() == CHECK_CHARACTER_NORMALIZATION)
    {
        if(value)
            m_flags |= FEATURE_CHECK_CHARACTER_NORMALIZATION;
        else
            m_flags &= ~FEATURE_CHECK_CHARACTER_NORMALIZATION;
    }
    else if(temp.lower() == DATATYPE_NORMALIZATION)
    {
        if(value)
        {
            m_flags |= FEATURE_DATATYPE_NORMALIZATION;
            m_flags |= FEATURE_VALIDATE;
        }
        else
            m_flags &= ~FEATURE_DATATYPE_NORMALIZATION;
    }
    else if(temp.lower() == ENTITIES)
    {
        if(value)
            m_flags |= FEATURE_ENTITIES;
        else
            m_flags &= ~FEATURE_ENTITIES;
    }
    else if(temp.lower() == ELEMENT_CONTENT_WHITESPACE)
    {
        if(value)
            m_flags |= FEATURE_WHITESPACE_IN_ELEMENT_CONTENT;
        else
            m_flags &= ~FEATURE_WHITESPACE_IN_ELEMENT_CONTENT;
    }
    else if(temp.lower() == NAMESPACES)
    {
        if(value)
            m_flags |= FEATURE_NAMESPACES;
        else
            m_flags &= ~FEATURE_NAMESPACES;
    }
    else if(temp.lower() == NAMESPACE_DECLARATIONS)
    {
        if(value)
            m_flags |= FEATURE_NAMESPACE_DECLARATIONS;
        else
            m_flags &= ~FEATURE_NAMESPACE_DECLARATIONS;
    }
    else if(temp.lower() == INFOSET)
    {
        if(value)
        {
            m_flags &= ~FEATURE_ENTITIES;
            m_flags &= ~FEATURE_CDATA_SECTIONS;
            m_flags &= ~FEATURE_VALIDATE_IF_SCHEMA;
            m_flags &= ~FEATURE_DATATYPE_NORMALIZATION;
            m_flags |= FEATURE_COMMENTS;
            m_flags |= FEATURE_NAMESPACES;
            m_flags |= FEATURE_WHITESPACE_IN_ELEMENT_CONTENT;
            m_flags |= FEATURE_WELL_FORMED;
            m_flags |= FEATURE_NAMESPACE_DECLARATIONS;
        }
    }
    else if(temp.lower() == CANONICAL_FORM)
    {
        if(value)
        {
            m_flags |= FEATURE_CANONICAL_FORM;
            m_flags &= ~FEATURE_ENTITIES;
            m_flags &= ~FEATURE_CDATA_SECTIONS;
            m_flags &= ~FEATURE_NORMALIZE_CHARACTERS;
            m_flags |= FEATURE_NAMESPACES;
            m_flags |= FEATURE_NAMESPACE_DECLARATIONS;
            m_flags |= FEATURE_WELL_FORMED;
            m_flags |= FEATURE_WHITESPACE_IN_ELEMENT_CONTENT;
        }
    }
    else if(temp.lower() == NORMALIZE_CHARACTERS)
    {
        if(value)
            m_flags |= FEATURE_NORMALIZE_CHARACTERS;
        else
            m_flags &= ~FEATURE_NORMALIZE_CHARACTERS;
    }
    else if(temp.lower() == SPLIT_CDATA_SECTIONS)
    {
        if(value)
            m_flags |= FEATURE_SPLIT_CDATA_SECTIONS;
        else
            m_flags &= ~FEATURE_SPLIT_CDATA_SECTIONS;
    }
    else if(temp.lower() == VALIDATE)
    {
        if(value)
        {
            m_flags |= FEATURE_VALIDATE;
            m_flags &= ~FEATURE_VALIDATE_IF_SCHEMA;
        }
        else
            m_flags &= ~FEATURE_VALIDATE;
    }
    else if(temp.lower() == VALIDATE_IF_SCHEMA)
    {
        if(value)
        {
            m_flags |= FEATURE_VALIDATE_IF_SCHEMA;
            m_flags &= ~FEATURE_VALIDATE;
        }
        else
            m_flags &= ~FEATURE_VALIDATE_IF_SCHEMA;
    }
    else if(temp.lower() == WELL_FORMED)
    {
        if(value)
            m_flags |= FEATURE_WELL_FORMED;
        else
            m_flags &= ~FEATURE_WELL_FORMED;
    }
    else if(temp.lower() == CHARSET_OVERRIDES_XML_ENCODING)
    {
        if(value)
            m_flags |= FEATURE_CHARSET_OVERRIDES_XML_ENCODING;
        else
            m_flags &= ~FEATURE_CHARSET_OVERRIDES_XML_ENCODING;
    }
    else if(temp.lower() == DISALLOW_DOCTYPE)
    {
        if(value)
            m_flags |= FEATURE_DISALLOW_DOCTYPE;
        else
            m_flags &= ~FEATURE_DISALLOW_DOCTYPE;
    }
    else if(temp.lower() == IGNORE_UNKNOWN_CD)
    {
        if(value)
            m_flags |= FEATURE_IGNORE_UNKNOWN_CD;
        else
            m_flags &= ~FEATURE_IGNORE_UNKNOWN_CD;
    }
    else if(temp.lower() == SUPPORTED_MEDIA_TYPE_ONLY)
    {
        if(value)
            m_flags |= FEATURE_SUPPORTED_MEDIA_TYPE_ONLY;
        else
            m_flags &= ~FEATURE_SUPPORTED_MEDIA_TYPE_ONLY;
    }
    else if(temp.lower() == DISCARD_DEFAULT_CONTENT)
    {
        if(value)
            m_flags |= FEATURE_DISCARD_DEFAULT_CONTENT;
        else
            m_flags &= ~FEATURE_DISCARD_DEFAULT_CONTENT;
    }
    else if(temp.lower() == FORMAT_PRETTY_PRINT)
    {
        if(value)
            m_flags |= FEATURE_FORMAT_PRETTY_PRINT;
        else
            m_flags &= ~FEATURE_FORMAT_PRETTY_PRINT;
    }
    else if(temp.lower() == XML_DECLARATION)
    {
        if(value)
            m_flags |= FEATURE_XML_DECLARATION;
        else
            m_flags &= ~FEATURE_XML_DECLARATION;
    }
}

DOMUserDataImpl *DOMConfigurationImpl::getParameter(DOMStringImpl *name) const
{
    if(!name)
        return 0;

    DOMString temp(name);
    if(temp.lower() == CANONICAL_FORM)
        return new DOMUserDataImpl(getParameter(FEATURE_CANONICAL_FORM));
    else if(temp.lower() == CDATA_SECTIONS)
        return new DOMUserDataImpl(getParameter(FEATURE_CDATA_SECTIONS));
    else if(temp.lower() == CHECK_CHARACTER_NORMALIZATION)
        return new DOMUserDataImpl(getParameter(FEATURE_CHECK_CHARACTER_NORMALIZATION));
    else if(temp.lower() == COMMENTS)
        return new DOMUserDataImpl(getParameter(FEATURE_COMMENTS));
    else if(temp.lower() == DATATYPE_NORMALIZATION)
        return new DOMUserDataImpl(getParameter(FEATURE_DATATYPE_NORMALIZATION));
    else if(temp.lower() ==    ELEMENT_CONTENT_WHITESPACE)
        return new DOMUserDataImpl(getParameter(FEATURE_WHITESPACE_IN_ELEMENT_CONTENT));
    else if(temp.lower() ==    ENTITIES)
        return new DOMUserDataImpl(getParameter(FEATURE_ENTITIES));
    else if(temp.lower() ==    INFOSET)
    {
        return new DOMUserDataImpl(!getParameter(FEATURE_ENTITIES) && 
                                    !getParameter(FEATURE_CDATA_SECTIONS) &&
                                    !getParameter(FEATURE_VALIDATE_IF_SCHEMA) &&
                                    !getParameter(FEATURE_DATATYPE_NORMALIZATION) &&
                                    getParameter(FEATURE_COMMENTS) &&
                                    getParameter(FEATURE_NAMESPACES) &&
                                    getParameter(FEATURE_WHITESPACE_IN_ELEMENT_CONTENT) &&
                                    getParameter(FEATURE_NAMESPACE_DECLARATIONS));
    }
    else if(temp.lower() ==    NAMESPACES)
        return new DOMUserDataImpl(getParameter(FEATURE_NAMESPACES));
    else if(temp.lower() ==    NAMESPACE_DECLARATIONS)
        return new DOMUserDataImpl(getParameter(FEATURE_NAMESPACE_DECLARATIONS));
    else if(temp.lower() == NORMALIZE_CHARACTERS)
        return new DOMUserDataImpl(getParameter(FEATURE_NORMALIZE_CHARACTERS));
    else if(temp.lower() ==    SPLIT_CDATA_SECTIONS)
        return new DOMUserDataImpl(getParameter(FEATURE_SPLIT_CDATA_SECTIONS));
    else if(temp.lower() ==    VALIDATE)
        return new DOMUserDataImpl(getParameter(FEATURE_VALIDATE));
    else if(temp.lower() ==    VALIDATE_IF_SCHEMA)
        return new DOMUserDataImpl(getParameter(FEATURE_VALIDATE_IF_SCHEMA));
    else if(temp.lower() ==    WELL_FORMED)
        return new DOMUserDataImpl(getParameter(FEATURE_WELL_FORMED));
    else if(temp.lower() ==    CHARSET_OVERRIDES_XML_ENCODING)
        return new DOMUserDataImpl(getParameter(FEATURE_CHARSET_OVERRIDES_XML_ENCODING));
    else if(temp.lower() ==    DISALLOW_DOCTYPE)
        return new DOMUserDataImpl(getParameter(FEATURE_DISALLOW_DOCTYPE));
    else if(temp.lower() ==    IGNORE_UNKNOWN_CD)
        return new DOMUserDataImpl(getParameter(FEATURE_IGNORE_UNKNOWN_CD));
    else if(temp.lower() ==    SUPPORTED_MEDIA_TYPE_ONLY)
        return new DOMUserDataImpl(getParameter(FEATURE_SUPPORTED_MEDIA_TYPE_ONLY));
    else if(temp.lower() ==    DISCARD_DEFAULT_CONTENT)
        return new DOMUserDataImpl(getParameter(FEATURE_DISCARD_DEFAULT_CONTENT));
    else if(temp.lower() ==    FORMAT_PRETTY_PRINT)
        return new DOMUserDataImpl(getParameter(FEATURE_FORMAT_PRETTY_PRINT));
    else if(temp.lower() ==    XML_DECLARATION)
        return new DOMUserDataImpl(getParameter(FEATURE_XML_DECLARATION));

    throw new DOMExceptionImpl(NOT_FOUND_ERR);
}

void DOMConfigurationImpl::setParameter(int flag, bool b)
{
    if(b)
        m_flags |= flag;
    else
        m_flags &= ~flag;
}

bool DOMConfigurationImpl::getParameter(int flag) const
{
    return (m_flags & flag) > 0;
}

bool DOMConfigurationImpl::canSetParameter(DOMStringImpl *name, DOMUserDataImpl *value) const
{
    if(!value)
        return true;

    if(!name)
        return false;

    DOMString temp(name);
    if(temp.lower() == ERROR_HANDLER)
        return true;
    else if(temp.lower() == CANONICAL_FORM)
        return true;
    else if(temp.lower() == CDATA_SECTIONS)
        return true;
    else if(temp.lower() == CHECK_CHARACTER_NORMALIZATION)
        return true;
    else if(temp.lower() == COMMENTS)
        return true;
    else if(temp.lower() == DATATYPE_NORMALIZATION)
        return true;
    else if(temp.lower() == ELEMENT_CONTENT_WHITESPACE)
        return true;
    else if(temp.lower() ==    ENTITIES)
        return true;
    else if(temp.lower() ==    ERROR_HANDLER)
        return true;
    else if(temp.lower() ==    INFOSET)
        return true;
    else if(temp.lower() ==    NAMESPACE_DECLARATIONS)
        return true;
    else if(temp.lower() ==    NAMESPACES)
        return true;
    else if(temp.lower() == NORMALIZE_CHARACTERS)
        return true;
    else if(temp.lower() == SCHEMA_LOCATION)
        return true;
    else if(temp.lower() == SCHEMA_TYPE)
        return true;
    else if(temp.lower() ==    SPLIT_CDATA_SECTIONS)
        return true;
    else if(temp.lower() ==    VALIDATE)
        return true;
    else if(temp.lower() ==    VALIDATE_IF_SCHEMA)
        return true;
    else if(temp.lower() ==    WELL_FORMED)
        return true;
    else if(temp.lower() ==    CHARSET_OVERRIDES_XML_ENCODING)
        return true;
    else if(temp.lower() ==    DISALLOW_DOCTYPE)
        return true;
    else if(temp.lower() ==    IGNORE_UNKNOWN_CD)
        return true;
//    else if(temp.lower() ==    RESOURCE_RESOLVER)
//        return true;
    else if(temp.lower() ==    SUPPORTED_MEDIA_TYPE_ONLY)
        return true;
    else if(temp.lower() ==    DISCARD_DEFAULT_CONTENT)
        return true;
    else if(temp.lower() ==    FORMAT_PRETTY_PRINT)
        return true;
    else if(temp.lower() ==    XML_DECLARATION)
        return true;

    return false;
}

DOMStringListImpl *DOMConfigurationImpl::parameterNames() const
{
    if(!m_paramNames)
    {
        m_paramNames = new DOMStringListImpl();
        m_paramNames->ref();

        m_paramNames->appendItem(CANONICAL_FORM.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(CDATA_SECTIONS.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(CHECK_CHARACTER_NORMALIZATION.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(COMMENTS.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(DATATYPE_NORMALIZATION.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(ELEMENT_CONTENT_WHITESPACE.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(ENTITIES.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(ERROR_HANDLER.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(INFOSET.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(NAMESPACES.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(NAMESPACE_DECLARATIONS.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(NORMALIZE_CHARACTERS.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(SCHEMA_LOCATION.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(SCHEMA_TYPE.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(SPLIT_CDATA_SECTIONS.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(VALIDATE.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(VALIDATE_IF_SCHEMA.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(WELL_FORMED.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(CHARSET_OVERRIDES_XML_ENCODING.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(DISALLOW_DOCTYPE.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(IGNORE_UNKNOWN_CD.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(RESOURCE_RESOLVER.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(SUPPORTED_MEDIA_TYPE_ONLY.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(DISCARD_DEFAULT_CONTENT.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(FORMAT_PRETTY_PRINT.handle());
        m_paramNames->getLast()->ref();
        m_paramNames->appendItem(XML_DECLARATION.handle());
        m_paramNames->getLast()->ref();
    }

    return m_paramNames;
}

DOMErrorHandlerImpl *DOMConfigurationImpl::errHandler() const
{
    return m_errHandler;
}

DOMStringImpl *DOMConfigurationImpl::normalizeCharacters(DOMStringImpl *dataImpl) const
{
    // TODO : possibly report error if check-character-normalization is set
    if(!getParameter(FEATURE_NORMALIZE_CHARACTERS))
        return dataImpl;

    DOMString data(dataImpl);
    QString temp = data.string();
#ifndef APPLE_COMPILE_HACK
    temp.compose();
#endif
    return new DOMStringImpl(temp.unicode(), temp.length());
}

// vim:ts=4:noet
