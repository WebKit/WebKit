/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
#ifndef HTML_OBJECTIMPL_H
#define HTML_OBJECTIMPL_H

#include "html_elementimpl.h"
#include "xml/dom_stringimpl.h"
#include "java/kjavaappletcontext.h"

#include <qstringlist.h>

class KHTMLView;

// -------------------------------------------------------------------------
namespace DOM {

class HTMLFormElementImpl;
class DOMStringImpl;

class HTMLAppletElementImpl : public HTMLElementImpl
{
public:
    HTMLAppletElementImpl(DocumentPtr *doc);

    ~HTMLAppletElementImpl();

    virtual Id id() const;

    virtual void parseAttribute(AttributeImpl *token);
    virtual void attach();
    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);

    bool getMember(const QString &, JType &, QString &);
    bool callMember(const QString &, const QStringList &, JType &, QString &);
protected:
    khtml::VAlign valign;
};

// -------------------------------------------------------------------------

class HTMLEmbedElementImpl : public HTMLElementImpl
{
public:
    HTMLEmbedElementImpl(DocumentPtr *doc);

    ~HTMLEmbedElementImpl();

    virtual Id id() const;

    virtual void parseAttribute(AttributeImpl *attr);

    virtual void attach();
    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);

    QString url;
    QString pluginPage;
    QString serviceType;
    bool hidden;
};

// -------------------------------------------------------------------------

class HTMLObjectElementImpl : public HTMLElementImpl
{
public:
    HTMLObjectElementImpl(DocumentPtr *doc);

    ~HTMLObjectElementImpl();

    virtual Id id() const;

    HTMLFormElementImpl *form() const;

    virtual void parseAttribute(AttributeImpl *token);

    virtual void attach();
    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void detach();

    virtual void recalcStyle( StyleChange ch );

    DocumentImpl* contentDocument() const;

    QString serviceType;
    QString url;
    QString classId;
    bool needWidgetUpdate;
};

// -------------------------------------------------------------------------

class HTMLParamElementImpl : public HTMLElementImpl
{
    friend class HTMLAppletElementImpl;
public:
    HTMLParamElementImpl(DocumentPtr *doc);

    ~HTMLParamElementImpl();

    virtual Id id() const;

    virtual void parseAttribute(AttributeImpl *token);

    QString name() const { if(!m_name) return QString::null; return QString(m_name->s, m_name->l); }
    QString value() const { if(!m_value) return QString::null; return QString(m_value->s, m_value->l); }

 protected:
    DOMStringImpl *m_name;
    DOMStringImpl *m_value;
};

};
#endif
