/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
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
 * $Id$
 */
#include "html_objectimpl.h"

#include "khtml_part.h"
#include "dom_string.h"
#include "htmlhashes.h"
#include "khtmlview.h"
#include <qstring.h>
#include <qmap.h>
#include <kdebug.h>

#include "xml/dom_docimpl.h"
#include "css/cssstyleselector.h"
#include "css/csshelper.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "rendering/render_applet.h"
#include "rendering/render_frames.h"
#include "xml/dom2_eventsimpl.h"

using namespace DOM;
using namespace khtml;

// -------------------------------------------------------------------------

HTMLAppletElementImpl::HTMLAppletElementImpl(DocumentPtr *doc)
  : HTMLElementImpl(doc)
{
    codeBase = 0;
    code = 0;
    name = 0;
    archive = 0;
}

HTMLAppletElementImpl::~HTMLAppletElementImpl()
{
    if(codeBase) codeBase->deref();
    if(code) code->deref();
    if(name) name->deref();
    if(archive) archive->deref();
}

const DOMString HTMLAppletElementImpl::nodeName() const
{
    return "APPLET";
}

ushort HTMLAppletElementImpl::id() const
{
    return ID_APPLET;
}

void HTMLAppletElementImpl::parseAttribute(AttrImpl *attr)
{
    switch( attr->attrId )
    {
    case ATTR_CODEBASE:
        codeBase = attr->val();
        codeBase->ref();
        break;
    case ATTR_ARCHIVE:
        archive = attr->val();
        archive->ref();
        break;
    case ATTR_CODE:
        code = attr->val();
        code->ref();
        break;
    case ATTR_OBJECT:
        break;
    case ATTR_ALT:
        break;
    case ATTR_NAME:
        name = attr->val();
        name->ref();
        break;
    case ATTR_WIDTH:
        addCSSLength(CSS_PROP_WIDTH, attr->value());
        break;
    case ATTR_HEIGHT:
        addCSSLength(CSS_PROP_HEIGHT, attr->value());
        break;
    case ATTR_ALIGN:
	addHTMLAlignment( attr->value() );
	break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLAppletElementImpl::attach()
{
  setStyle(ownerDocument()->styleSelector()->styleForElement(this));
  if(!code)
      return;

  khtml::RenderObject *r = _parent->renderer();
  RenderWidget *f = 0;

  if(r && m_style->display() != NONE) {
      view = ownerDocument()->view();

      if( view->part()->javaEnabled() )
      {
          QMap<QString, QString> args;

          args.insert( "code", QString(code->s, code->l));
          if(codeBase)
              args.insert( "codeBase", QString(codeBase->s, codeBase->l) );
          if(name)
              args.insert( "name", QString(name->s, name->l) );
          if(archive)
              args.insert( "archive", QString(archive->s, archive->l) );

          if(!view->part()->baseURL().isEmpty())
              args.insert( "baseURL", view->part()->baseURL().url() );
          else
              args.insert( "baseURL", view->part()->url().url() );

          f = new RenderApplet(view, args, this);
      }
      else
          f = new RenderEmptyApplet(view);
  }

  if(f)
  {
      m_render = f;
      m_render->setStyle(m_style);
      r->addChild(m_render, nextRenderer());
  }
  HTMLElementImpl::attach();
}

void HTMLAppletElementImpl::detach()
{
    view = 0;
    HTMLElementImpl::detach();
}

// -------------------------------------------------------------------------

HTMLEmbedElementImpl::HTMLEmbedElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLEmbedElementImpl::~HTMLEmbedElementImpl()
{
}

const DOMString HTMLEmbedElementImpl::nodeName() const
{
    return "EMBED";
}

ushort HTMLEmbedElementImpl::id() const
{
    return ID_EMBED;
}

void HTMLEmbedElementImpl::parseAttribute(AttrImpl *attr)
{
  DOM::DOMStringImpl *stringImpl = attr->val();
  QString val = QConstString( stringImpl->s, stringImpl->l ).string();

  // export parameter
#ifdef APPLE_CHANGES
    // Need to make a deep copy here or "+" operator will attempt to modify
    // QConstString attribute name
  QString attrStr = attr->name().string().copy() + "=\"" + val + "\"";
#else /* APPLE_CHANGES not defined */
  QString attrStr = attr->name().string() + "=\"" + val + "\"";
#endif /* APPLE_CHANGES not defined */
  param.append( attrStr );
  int pos;
  switch ( attr->attrId )
  {
     case ATTR_TYPE:
        serviceType = val.lower();
        pos = serviceType.find( ";" );
        if ( pos!=-1 )
            serviceType = serviceType.left( pos );
        break;
     case ATTR_CODE:
     case ATTR_SRC:
#ifdef APPLE_CHANGES
         url = khtml::parseURL(attr->val()).string().copy();
#else /* APPLE_CHANGES not defined */
         url = khtml::parseURL(attr->val()).string();
#endif /* APPLE_CHANGES not defined */
         break;
     case ATTR_WIDTH:
        addCSSLength( CSS_PROP_WIDTH, attr->value() );
        break;
     case ATTR_HEIGHT:
        addCSSLength( CSS_PROP_HEIGHT, attr->value());
        break;
     case ATTR_BORDER:
        addCSSLength(CSS_PROP_BORDER_WIDTH, attr->value());
        addCSSProperty( CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID );
        addCSSProperty( CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID );
        addCSSProperty( CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID );
        addCSSProperty( CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID );
        break;
     case ATTR_VSPACE:
        addCSSLength(CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(CSS_PROP_MARGIN_BOTTOM, attr->value());
        break;
     case ATTR_HSPACE:
        addCSSLength(CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(CSS_PROP_MARGIN_RIGHT, attr->value());
        break;
     case ATTR_ALIGN:
	addHTMLAlignment( attr->value() );
	break;
     case ATTR_VALIGN:
        addCSSProperty(CSS_PROP_VERTICAL_ALIGN, attr->value());
        break;
     case ATTR_PLUGINPAGE:
     case ATTR_PLUGINSPAGE:
        pluginPage = val;
        break;
     case ATTR_HIDDEN:
        if (val.lower()=="yes" || val.lower()=="true")
           hidden = true;
        else
           hidden = false;
        break;
     default:
        HTMLElementImpl::parseAttribute( attr );
  }
}

void HTMLEmbedElementImpl::attach()
{
   KHTMLView* w = ownerDocument()->view();
   setStyle(ownerDocument()->styleSelector()->styleForElement( this ));
   khtml::RenderObject *r = _parent->renderer();
   RenderPartObject* p = 0;
   if ( r && m_style->display() != NONE ) {
       if (w->part()->pluginsEnabled())
       {
           if ( _parent->id()!=ID_OBJECT )
           {
               p = new RenderPartObject( w, this );
               m_render = p;
               m_render->setStyle(m_style);
               r->addChild( m_render, nextRenderer() );
           } else
               r->setStyle(m_style);
       }
   }


   HTMLElementImpl::attach();

  if ( p )
      p->updateWidget();
}

void HTMLEmbedElementImpl::detach()
{
  HTMLElementImpl::detach();
}

// -------------------------------------------------------------------------

HTMLObjectElementImpl::HTMLObjectElementImpl(DocumentPtr *doc) : HTMLElementImpl(doc)
{
    needWidgetUpdate = false;
}

HTMLObjectElementImpl::~HTMLObjectElementImpl()
{
}

const DOMString HTMLObjectElementImpl::nodeName() const
{
    return "OBJECT";
}

ushort HTMLObjectElementImpl::id() const
{
    return ID_OBJECT;
}

HTMLFormElementImpl *HTMLObjectElementImpl::form() const
{
  return 0;
}

void HTMLObjectElementImpl::parseAttribute(AttrImpl *attr)
{
  DOM::DOMStringImpl *stringImpl = attr->val();
  QString val = QConstString( stringImpl->s, stringImpl->l ).string();
  int pos;
  switch ( attr->attrId )
  {
    case ATTR_TYPE:
      serviceType = val.lower();
      pos = serviceType.find( ";" );
      if ( pos!=-1 )
          serviceType = serviceType.left( pos );
      needWidgetUpdate = true;
      break;
    case ATTR_DATA:
      url = khtml::parseURL(  val ).string();
      needWidgetUpdate = true;
      break;
    case ATTR_WIDTH:
      addCSSLength( CSS_PROP_WIDTH, attr->value());
      break;
    case ATTR_HEIGHT:
      addCSSLength( CSS_PROP_HEIGHT, attr->value());
      break;
    case ATTR_CLASSID:
      classId = val;
      needWidgetUpdate = true;
      break;
    case ATTR_ONLOAD: // ### support load/unload on object elements
        setHTMLEventListener(EventImpl::LOAD_EVENT,
	    ownerDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONUNLOAD:
        setHTMLEventListener(EventImpl::UNLOAD_EVENT,
	    ownerDocument()->createHTMLEventListener(attr->value().string()));
        break;
    default:
      HTMLElementImpl::parseAttribute( attr );
  }
}

void HTMLObjectElementImpl::attach()
{
  KHTMLView* w = ownerDocument()->view();
  setStyle(ownerDocument()->styleSelector()->styleForElement( this ));

  khtml::RenderObject *r = _parent->renderer();
  if ( r && m_style->display() != NONE ) {
      if (w->part()->pluginsEnabled())
      {
          RenderPartObject *p = new RenderPartObject( w, this );
          m_render = p;
          m_render->setStyle(m_style);
          r->addChild( m_render, nextRenderer() );
      }
  }

  HTMLElementImpl::attach();

  // ### do this when we are actually finished loading instead
  dispatchHTMLEvent(EventImpl::LOAD_EVENT,false,false);
}

void HTMLObjectElementImpl::detach()
{
  HTMLElementImpl::detach();
  // ### do this when we are actualy removed from document instead
  dispatchHTMLEvent(EventImpl::UNLOAD_EVENT,false,false);
}

void HTMLObjectElementImpl::applyChanges(bool top, bool force)
{
    if (needWidgetUpdate) {
        if(m_render && strcmp( m_render->renderName(),  "RenderPartObject" ) == 0 )
            static_cast<RenderPartObject*>(m_render)->updateWidget();
        needWidgetUpdate = false;
    }
    HTMLElementImpl::applyChanges(top,force);
}

// -------------------------------------------------------------------------

HTMLParamElementImpl::HTMLParamElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
    m_name = 0;
    m_value = 0;
}

HTMLParamElementImpl::~HTMLParamElementImpl()
{
    if(m_name) m_name->deref();
    if(m_value) m_value->deref();
}

const DOMString HTMLParamElementImpl::nodeName() const
{
    return "PARAM";
}

ushort HTMLParamElementImpl::id() const
{
    return ID_PARAM;
}

void HTMLParamElementImpl::parseAttribute(AttrImpl *attr)
{
    switch( attr->attrId )
    {
    case ATTR_NAME:
        m_name = attr->val();
        m_name->ref();
        break;
    case ATTR_VALUE:
        m_value = attr->val();
        m_value->ref();
        break;
    }
}
