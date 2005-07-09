/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004 Apple Computer, Inc.
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
 */
#include "html/html_objectimpl.h"

#include "khtml_part.h"
#include "dom/dom_string.h"
#include "misc/htmlhashes.h"
#include "khtmlview.h"
#include <qstring.h>
#include <qmap.h>
#include <kdebug.h>

#include "xml/dom_docimpl.h"
#include "css/cssstyleselector.h"
#include "css/csshelper.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "html/html_formimpl.h"
#include "rendering/render_applet.h"
#include "rendering/render_frames.h"
#include "rendering/render_image.h"
#include "xml/dom2_eventsimpl.h"

#ifndef Q_WS_QWS // We don't have Java in Qt Embedded
#include "java/kjavaappletwidget.h"
#include "java/kjavaappletcontext.h"
#endif

#if APPLE_CHANGES
#include "KWQKHTMLPart.h"
#endif

using namespace khtml;

namespace DOM {

// -------------------------------------------------------------------------

HTMLAppletElementImpl::HTMLAppletElementImpl(DocumentPtr *doc)
  : HTMLElementImpl(HTMLNames::applet(), doc)
{
    appletInstance = 0;
    m_allParamsAvailable = false;
}

HTMLAppletElementImpl::~HTMLAppletElementImpl()
{
    delete appletInstance;
}

bool HTMLAppletElementImpl::checkDTD(const NodeImpl* newChild)
{
    return newChild->hasTagName(HTMLNames::param()) || HTMLElementImpl::checkDTD(newChild);
}

bool HTMLAppletElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    switch (attr) {
        case ATTR_WIDTH:
        case ATTR_HEIGHT:
        case ATTR_VSPACE:
        case ATTR_HSPACE:
            result = eUniversal;
            return false;
        case ATTR_ALIGN:
            result = eReplaced; // Share with <img> since the alignment behavior is the same.
            return false;
        default:
            break;
    }
    
    return HTMLElementImpl::mapToEntry(attr, result);
}

void HTMLAppletElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    switch (attr->id()) {
    case ATTR_ALT:
    case ATTR_ARCHIVE:
    case ATTR_CODE:
    case ATTR_CODEBASE:
    case ATTR_MAYSCRIPT:
    case ATTR_NAME:
    case ATTR_OBJECT:
        break;
    case ATTR_WIDTH:
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
        break;
    case ATTR_HEIGHT:
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
        break;
    case ATTR_VSPACE:
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
        break;
    case ATTR_HSPACE:
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
        break;
    case ATTR_ALIGN:
        addHTMLAlignment(attr);
        break;
    default:
        HTMLElementImpl::parseMappedAttribute(attr);
    }
}

bool HTMLAppletElementImpl::rendererIsNeeded(RenderStyle *style)
{
    return !getAttribute(ATTR_CODE).isNull();
}

RenderObject *HTMLAppletElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
#ifndef Q_WS_QWS // FIXME(E)? I don't think this is possible with Qt Embedded...
    KHTMLPart *part = getDocument()->part();

    if( part && part->javaEnabled() )
    {
	QMap<QString, QString> args;

	args.insert( "code", getAttribute(ATTR_CODE).string());
	DOMString codeBase = getAttribute(ATTR_CODEBASE);
	if(!codeBase.isNull())
	    args.insert( "codeBase", codeBase.string() );
	DOMString name = getDocument()->htmlMode() != DocumentImpl::XHtml ?
			 getAttribute(ATTR_NAME) : getAttribute(ATTR_ID);
	if(!name.isNull())
	    args.insert( "name", name.string() );
	DOMString archive = getAttribute(ATTR_ARCHIVE);
	if(!archive.isNull())
	    args.insert( "archive", archive.string() );

	args.insert( "baseURL", getDocument()->baseURL() );

        DOMString mayScript = getAttribute(ATTR_MAYSCRIPT);
        if (!mayScript.isNull())
            args.insert("mayScript", mayScript.string());

        // Other arguments (from <PARAM> tags) are added later.
        
        return new (getDocument()->renderArena()) RenderApplet(this, args);
    }

    // ### remove me. we should never show an empty applet, instead
    // render the alternative content given by the webpage
    return new (getDocument()->renderArena()) RenderEmptyApplet(this);
#else
    return 0;
#endif
}

bool HTMLAppletElementImpl::getMember(const QString & name, JType & type, QString & val) {
#if APPLE_CHANGES
    return false;
#else
#ifndef Q_WS_QWS // We don't have Java in Qt Embedded
    if ( !m_render || !m_render->isApplet() )
        return false;
    KJavaAppletWidget *w = static_cast<KJavaAppletWidget*>(static_cast<RenderApplet*>(m_render)->widget());
    return (w && w->applet() && w->applet()->getMember(name, type, val));
#else
    return false;
#endif
#endif
}

bool HTMLAppletElementImpl::callMember(const QString & name, const QStringList & args, JType & type, QString & val) {
#if APPLE_CHANGES
    return false;
#else
#ifndef Q_WS_QWS // We don't have Java in Qt Embedded
    if ( !m_render || !m_render->isApplet() )
        return false;
    KJavaAppletWidget *w = static_cast<KJavaAppletWidget*>(static_cast<RenderApplet*>(m_render)->widget());
    return (w && w->applet() && w->applet()->callMember(name, args, type, val));
#else
    return false;
#endif
#endif
}

#if APPLE_CHANGES
KJS::Bindings::Instance *HTMLAppletElementImpl::getAppletInstance() const
{
    KHTMLPart* part = getDocument()->part();
    if (!part || !part->javaEnabled())
        return 0;

    if (appletInstance)
        return appletInstance;
    
    RenderApplet *r = static_cast<RenderApplet*>(m_render);
    if (r) {
        r->createWidgetIfNecessary();
        if (r->widget()){
            // Call into the part (and over the bridge) to pull the Bindings::Instance
            // from the guts of the plugin.
            void *_view = r->widget()->getView();
            appletInstance = KWQ(part)->getAppletInstanceForView((NSView *)_view);
        }
    }
    return appletInstance;
}

void HTMLAppletElementImpl::closeRenderer()
{
    // The parser just reached </applet>, so all the params are available now.
    m_allParamsAvailable = true;
    if( m_render )
        m_render->setNeedsLayout(true); // This will cause it to create its widget & the Java applet
    
    HTMLElementImpl::closeRenderer();
}

bool HTMLAppletElementImpl::allParamsAvailable()
{
    return m_allParamsAvailable;
}
#endif

DOMString HTMLAppletElementImpl::align() const
{
    return getAttribute(ATTR_ALIGN);
}

void HTMLAppletElementImpl::setAlign(const DOMString &value)
{
    setAttribute(ATTR_ALIGN, value);
}

DOMString HTMLAppletElementImpl::alt() const
{
    return getAttribute(ATTR_ALT);
}

void HTMLAppletElementImpl::setAlt(const DOMString &value)
{
    setAttribute(ATTR_ALT, value);
}

DOMString HTMLAppletElementImpl::archive() const
{
    return getAttribute(ATTR_ARCHIVE);
}

void HTMLAppletElementImpl::setArchive(const DOMString &value)
{
    setAttribute(ATTR_ARCHIVE, value);
}

DOMString HTMLAppletElementImpl::code() const
{
    return getAttribute(ATTR_CODE);
}

void HTMLAppletElementImpl::setCode(const DOMString &value)
{
    setAttribute(ATTR_CODE, value);
}

DOMString HTMLAppletElementImpl::codeBase() const
{
    return getAttribute(ATTR_CODEBASE);
}

void HTMLAppletElementImpl::setCodeBase(const DOMString &value)
{
    setAttribute(ATTR_CODEBASE, value);
}

DOMString HTMLAppletElementImpl::height() const
{
    return getAttribute(ATTR_HEIGHT);
}

void HTMLAppletElementImpl::setHeight(const DOMString &value)
{
    setAttribute(ATTR_HEIGHT, value);
}

DOMString HTMLAppletElementImpl::hspace() const
{
    return getAttribute(ATTR_HSPACE);
}

void HTMLAppletElementImpl::setHspace(const DOMString &value)
{
    setAttribute(ATTR_HSPACE, value);
}

DOMString HTMLAppletElementImpl::name() const
{
    return getAttribute(ATTR_NAME);
}

void HTMLAppletElementImpl::setName(const DOMString &value)
{
    setAttribute(ATTR_NAME, value);
}

DOMString HTMLAppletElementImpl::object() const
{
    return getAttribute(ATTR_OBJECT);
}

void HTMLAppletElementImpl::setObject(const DOMString &value)
{
    setAttribute(ATTR_OBJECT, value);
}

DOMString HTMLAppletElementImpl::vspace() const
{
    return getAttribute(ATTR_VSPACE);
}

void HTMLAppletElementImpl::setVspace(const DOMString &value)
{
    setAttribute(ATTR_VSPACE, value);
}

DOMString HTMLAppletElementImpl::width() const
{
    return getAttribute(ATTR_WIDTH);
}

void HTMLAppletElementImpl::setWidth(const DOMString &value)
{
    setAttribute(ATTR_WIDTH, value);
}

// -------------------------------------------------------------------------

HTMLEmbedElementImpl::HTMLEmbedElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(HTMLNames::embed(), doc), embedInstance(0)
{}

HTMLEmbedElementImpl::~HTMLEmbedElementImpl()
{
}

bool HTMLEmbedElementImpl::checkDTD(const NodeImpl* newChild)
{
    return newChild->hasTagName(HTMLNames::param()) || HTMLElementImpl::checkDTD(newChild);
}

#if APPLE_CHANGES
KJS::Bindings::Instance *HTMLEmbedElementImpl::getEmbedInstance() const
{
    KHTMLPart* part = getDocument()->part();
    if (!part)
        return 0;

    if (embedInstance)
        return embedInstance;
    
    RenderPartObject *r = static_cast<RenderPartObject*>(m_render);
    if (r) {
        if (r->widget()){
            // Call into the part (and over the bridge) to pull the Bindings::Instance
            // from the guts of the Java VM.
            void *_view = r->widget()->getView();
            embedInstance = KWQ(part)->getEmbedInstanceForView((NSView *)_view);
            // Applet may specified with <embed> tag.
            if (!embedInstance)
                embedInstance = KWQ(part)->getAppletInstanceForView((NSView *)_view);
        }
    }
    return embedInstance;
}
#endif

bool HTMLEmbedElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    switch (attr) {
        case ATTR_WIDTH:
        case ATTR_HEIGHT:
        case ATTR_BORDER:
        case ATTR_VSPACE:
        case ATTR_HSPACE:
        case ATTR_VALIGN:
        case ATTR_HIDDEN:
            result = eUniversal;
            return false;
        case ATTR_ALIGN:
            result = eReplaced; // Share with <img> since the alignment behavior is the same.
            return false;
        default:
            break;
    }
    
    return HTMLElementImpl::mapToEntry(attr, result);
}

void HTMLEmbedElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
  QString val = attr->value().string();
  
  int pos;
  switch ( attr->id() )
  {
     case ATTR_TYPE:
        serviceType = val.lower();
        pos = serviceType.find( ";" );
        if ( pos!=-1 )
            serviceType = serviceType.left( pos );
        break;
     case ATTR_CODE:
     case ATTR_SRC:
         url = khtml::parseURL(attr->value()).string();
         break;
     case ATTR_WIDTH:
        addCSSLength( attr, CSS_PROP_WIDTH, attr->value() );
        break;
     case ATTR_HEIGHT:
        addCSSLength( attr, CSS_PROP_HEIGHT, attr->value());
        break;
     case ATTR_BORDER:
        addCSSLength(attr, CSS_PROP_BORDER_WIDTH, attr->value());
        addCSSProperty( attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID );
        addCSSProperty( attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID );
        addCSSProperty( attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID );
        addCSSProperty( attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID );
        break;
     case ATTR_VSPACE:
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
        break;
     case ATTR_HSPACE:
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
        break;
     case ATTR_ALIGN:
	addHTMLAlignment(attr);
	break;
     case ATTR_VALIGN:
        addCSSProperty(attr, CSS_PROP_VERTICAL_ALIGN, attr->value());
        break;
     case ATTR_PLUGINPAGE:
     case ATTR_PLUGINSPAGE:
        pluginPage = val;
        break;
     case ATTR_HIDDEN:
        if (val.lower()=="yes" || val.lower()=="true") {
            // FIXME: Not dynamic, but it's not really important that such a rarely-used
            // feature work dynamically.
            addCSSLength( attr, CSS_PROP_WIDTH, "0" );
            addCSSLength( attr, CSS_PROP_HEIGHT, "0" );
        }
        break;
     default:
        HTMLElementImpl::parseMappedAttribute( attr );
  }
}

bool HTMLEmbedElementImpl::rendererIsNeeded(RenderStyle *style)
{
    KHTMLPart *part = getDocument()->part();
    if (!part)
	return false;
    return part->pluginsEnabled() && !parentNode()->hasTagName(HTMLNames::object());
}

RenderObject *HTMLEmbedElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderPartObject(this);
}

void HTMLEmbedElementImpl::attach()
{
    HTMLElementImpl::attach();
    if (m_render) {
        static_cast<RenderPartObject*>(m_render)->updateWidget();
    }
}

bool HTMLEmbedElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->id() == ATTR_SRC;
}

// -------------------------------------------------------------------------

HTMLObjectElementImpl::HTMLObjectElementImpl(DocumentPtr *doc) 
#if APPLE_CHANGES
: HTMLElementImpl(HTMLNames::object(), doc), m_imageLoader(0), objectInstance(0)
#else
: HTMLElementImpl(HTMLNames::object(), doc), m_imageLoader(0)
#endif
{
    needWidgetUpdate = false;
    m_useFallbackContent = false;
}

HTMLObjectElementImpl::~HTMLObjectElementImpl()
{
    delete m_imageLoader;
}

bool HTMLObjectElementImpl::checkDTD(const NodeImpl* newChild)
{
    return newChild->hasTagName(HTMLNames::param()) || HTMLElementImpl::checkDTD(newChild);
}

#if APPLE_CHANGES
KJS::Bindings::Instance *HTMLObjectElementImpl::getObjectInstance() const
{
    KHTMLPart* part = getDocument()->part();
    if (!part)
        return 0;

    if (objectInstance)
        return objectInstance;

    if (RenderObject *r = m_render) {
        if (r->isWidget()) {
            if (QWidget *widget = static_cast<RenderWidget *>(r)->widget()) {
                if (NSView *view = widget->getView())  {
                    // Call into the part (and over the bridge) to pull the Bindings::Instance
                    // from the guts of the plugin.
                    objectInstance = KWQ(part)->getObjectInstanceForView(view);
                    // Applet may specified with <object> tag.
                    if (!objectInstance)
                        objectInstance = KWQ(part)->getAppletInstanceForView(view);
                }
            }
        }
    }

    return objectInstance;
}
#endif

HTMLFormElementImpl *HTMLObjectElementImpl::form() const
{
    for (NodeImpl *p = parentNode(); p != 0; p = p->parentNode()) {
        if (p->hasTagName(HTMLNames::form()))
            return static_cast<HTMLFormElementImpl *>(p);
    }
    
    return 0;
}

bool HTMLObjectElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    switch (attr) {
        case ATTR_WIDTH:
        case ATTR_HEIGHT:
        case ATTR_VSPACE:
        case ATTR_HSPACE:
            result = eUniversal;
            return false;
        case ATTR_ALIGN:
            result = eReplaced; // Share with <img> since the alignment behavior is the same.
            return false;
        default:
            break;
    }
    
    return HTMLElementImpl::mapToEntry(attr, result);
}

void HTMLObjectElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
  QString val = attr->value().string();
  int pos;
  switch ( attr->id() )
  {
    case ATTR_TYPE:
      serviceType = val.lower();
      pos = serviceType.find( ";" );
      if ( pos!=-1 )
          serviceType = serviceType.left( pos );
      if (m_render)
          needWidgetUpdate = true;
      if (!isImageType() && m_imageLoader) {
          delete m_imageLoader;
          m_imageLoader = 0;
      }
      break;
    case ATTR_DATA:
      url = khtml::parseURL(  val ).string();
      if (m_render)
          needWidgetUpdate = true;
      if (m_render && isImageType()) {
          if (!m_imageLoader)
              m_imageLoader = new HTMLImageLoader(this);
          m_imageLoader->updateFromElement();
      }
      break;
    case ATTR_WIDTH:
      addCSSLength( attr, CSS_PROP_WIDTH, attr->value());
      break;
    case ATTR_HEIGHT:
      addCSSLength( attr, CSS_PROP_HEIGHT, attr->value());
      break;
    case ATTR_VSPACE:
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
        break;
    case ATTR_HSPACE:
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
        break;
    case ATTR_ALIGN:
        addHTMLAlignment(attr);
        break;
    case ATTR_CLASSID:
      classId = val;
      if (m_render)
          needWidgetUpdate = true;
      break;
    case ATTR_ONLOAD: // ### support load/unload on object elements
        setHTMLEventListener(EventImpl::LOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONUNLOAD:
        setHTMLEventListener(EventImpl::UNLOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    default:
      HTMLElementImpl::parseMappedAttribute( attr );
  }
}

DocumentImpl* HTMLObjectElementImpl::contentDocument() const
{
    // ###
    return 0;
}

bool HTMLObjectElementImpl::rendererIsNeeded(RenderStyle *style)
{
    if (m_useFallbackContent || isImageType())
        return HTMLElementImpl::rendererIsNeeded(style);

    KHTMLPart* part = getDocument()->part();
    if (!part || !part->pluginsEnabled()) {
        return false;
    }
#if APPLE_CHANGES
    // Eventually we will merge with the better version of this check on the tip of tree.
    // Until then, just leave it out.
#else
    KURL u = getDocument()->completeURL(url);
    for (KHTMLPart* part = w->part()->parentPart(); part; part = part->parentPart())
        if (part->url() == u) {
            return false;
        }
#endif
    return true;
}

RenderObject *HTMLObjectElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    if (m_useFallbackContent)
        return RenderObject::createObject(this, style);
    if (isImageType())
        return new (arena) RenderImage(this);
    return new (arena) RenderPartObject(this);
}

void HTMLObjectElementImpl::attach()
{
    HTMLElementImpl::attach();

    if (m_render && !m_useFallbackContent) {
        if (isImageType()) {
            if (!m_imageLoader)
                m_imageLoader = new HTMLImageLoader(this);
            m_imageLoader->updateFromElement();
            if (renderer()) {
                RenderImage* imageObj = static_cast<RenderImage*>(renderer());
                imageObj->setImage(m_imageLoader->image());
            }
        } else {
            if (needWidgetUpdate) {
                // Set needWidgetUpdate to false before calling updateWidget because updateWidget may cause
                // this method or recalcStyle (which also calls updateWidget) to be called.
                needWidgetUpdate = false;
                static_cast<RenderPartObject*>(m_render)->updateWidget();
                dispatchHTMLEvent(EventImpl::LOAD_EVENT,false,false);
            } else {
                needWidgetUpdate = true;
                setChanged();
            }
        }
    }
}

void HTMLObjectElementImpl::detach()
{
    // Only bother with an unload event if we had a render object.  - dwh
    if (attached() && m_render && !m_useFallbackContent)
        // ### do this when we are actualy removed from document instead
        dispatchHTMLEvent(EventImpl::UNLOAD_EVENT,false,false);

  HTMLElementImpl::detach();
}

void HTMLObjectElementImpl::recalcStyle(StyleChange ch)
{
    if (!m_useFallbackContent && needWidgetUpdate && m_render && !isImageType()) {
        // Set needWidgetUpdate to false before calling updateWidget because updateWidget may cause
        // this method or attach (which also calls updateWidget) to be called.
        needWidgetUpdate = false;
        static_cast<RenderPartObject*>(m_render)->updateWidget();
        dispatchHTMLEvent(EventImpl::LOAD_EVENT,false,false);
    }
    HTMLElementImpl::recalcStyle(ch);
}

void HTMLObjectElementImpl::childrenChanged()
{
    if (inDocument() && !m_useFallbackContent) {
        needWidgetUpdate = true;
        setChanged();
    }
}

bool HTMLObjectElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return (attr->id() == ATTR_DATA || (attr->id() == ATTR_USEMAP && attr->value().domString()[0] != '#'));
}

bool HTMLObjectElementImpl::isImageType()
{
    if (serviceType.isEmpty() && url.startsWith("data:")) {
        // Extract the MIME type from the data URL.
        int index = url.find(';');
        if (index == -1)
            index = url.find(',');
        if (index != -1) {
            int len = index - 5;
            if (len > 0)
                serviceType = url.mid(5, len);
            else
                serviceType = "text/plain"; // Data URLs with no MIME type are considered text/plain.
        }
    }

    return canRenderImageType(serviceType);
}

void HTMLObjectElementImpl::renderFallbackContent()
{
    if (m_useFallbackContent)
        return;

    // Mark ourselves as using the fallback content.
    m_useFallbackContent = true;

    // Now do a detach and reattach.    
    // FIXME: Style gets recalculated which is suboptimal.
    detach();
    attach();
}

DOMString HTMLObjectElementImpl::code() const
{
    return getAttribute(ATTR_CODE);
}

void HTMLObjectElementImpl::setCode(const DOMString &value)
{
    setAttribute(ATTR_CODE, value);
}

DOMString HTMLObjectElementImpl::align() const
{
    return getAttribute(ATTR_ALIGN);
}

void HTMLObjectElementImpl::setAlign(const DOMString &value)
{
    setAttribute(ATTR_ALIGN, value);
}

DOMString HTMLObjectElementImpl::archive() const
{
    return getAttribute(ATTR_ARCHIVE);
}

void HTMLObjectElementImpl::setArchive(const DOMString &value)
{
    setAttribute(ATTR_ARCHIVE, value);
}

DOMString HTMLObjectElementImpl::border() const
{
    return getAttribute(ATTR_BORDER);
}

void HTMLObjectElementImpl::setBorder(const DOMString &value)
{
    setAttribute(ATTR_BORDER, value);
}

DOMString HTMLObjectElementImpl::codeBase() const
{
    return getAttribute(ATTR_CODEBASE);
}

void HTMLObjectElementImpl::setCodeBase(const DOMString &value)
{
    setAttribute(ATTR_CODEBASE, value);
}

DOMString HTMLObjectElementImpl::codeType() const
{
    return getAttribute(ATTR_CODETYPE);
}

void HTMLObjectElementImpl::setCodeType(const DOMString &value)
{
    setAttribute(ATTR_CODETYPE, value);
}

DOMString HTMLObjectElementImpl::data() const
{
    return getAttribute(ATTR_DATA);
}

void HTMLObjectElementImpl::setData(const DOMString &value)
{
    setAttribute(ATTR_DATA, value);
}

bool HTMLObjectElementImpl::declare() const
{
    return !getAttribute(ATTR_DECLARE).isNull();
}

void HTMLObjectElementImpl::setDeclare(bool declare)
{
    setAttribute(ATTR_DECLARE, declare ? "" : 0);
}

DOMString HTMLObjectElementImpl::height() const
{
    return getAttribute(ATTR_HEIGHT);
}

void HTMLObjectElementImpl::setHeight(const DOMString &value)
{
    setAttribute(ATTR_HEIGHT, value);
}

DOMString HTMLObjectElementImpl::hspace() const
{
    return getAttribute(ATTR_HSPACE);
}

void HTMLObjectElementImpl::setHspace(const DOMString &value)
{
    setAttribute(ATTR_HSPACE, value);
}

DOMString HTMLObjectElementImpl::name() const
{
    return getAttribute(ATTR_NAME);
}

void HTMLObjectElementImpl::setName(const DOMString &value)
{
    setAttribute(ATTR_NAME, value);
}

DOMString HTMLObjectElementImpl::standby() const
{
    return getAttribute(ATTR_STANDBY);
}

void HTMLObjectElementImpl::setStandby(const DOMString &value)
{
    setAttribute(ATTR_STANDBY, value);
}

long HTMLObjectElementImpl::tabIndex() const
{
    return getAttribute(ATTR_TABINDEX).toInt();
}

void HTMLObjectElementImpl::setTabIndex(long tabIndex)
{
    setAttribute(ATTR_TABINDEX, QString::number(tabIndex));
}

DOMString HTMLObjectElementImpl::type() const
{
    return getAttribute(ATTR_TYPE);
}

void HTMLObjectElementImpl::setType(const DOMString &value)
{
    setAttribute(ATTR_TYPE, value);
}

DOMString HTMLObjectElementImpl::useMap() const
{
    return getAttribute(ATTR_USEMAP);
}

void HTMLObjectElementImpl::setUseMap(const DOMString &value)
{
    setAttribute(ATTR_USEMAP, value);
}

DOMString HTMLObjectElementImpl::vspace() const
{
    return getAttribute(ATTR_VSPACE);
}

void HTMLObjectElementImpl::setVspace(const DOMString &value)
{
    setAttribute(ATTR_VSPACE, value);
}

DOMString HTMLObjectElementImpl::width() const
{
    return getAttribute(ATTR_WIDTH);
}

void HTMLObjectElementImpl::setWidth(const DOMString &value)
{
    setAttribute(ATTR_WIDTH, value);
}

// -------------------------------------------------------------------------

HTMLParamElementImpl::HTMLParamElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(HTMLNames::param(), doc)
{
}

HTMLParamElementImpl::~HTMLParamElementImpl()
{
}

void HTMLParamElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    switch( attr->id() )
    {
    case ATTR_ID:
        // Must call base class so that hasID bit gets set.
        HTMLElementImpl::parseMappedAttribute(attr);
        if (getDocument()->htmlMode() != DocumentImpl::XHtml) break;
        // fall through
    case ATTR_NAME:
        m_name = attr->value();
        break;
    case ATTR_VALUE:
        m_value = attr->value();
        break;
    }
}

bool HTMLParamElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    if (attr->id() == ATTR_VALUE) {
        AttributeImpl *attr = attributes()->getAttributeItem(ATTR_NAME);
        if (attr) {
            DOMString value = attr->value().string().lower();
            if (value == "src" || value == "movie" || value == "data") {
                return true;
            }
        }
    }
    return false;
}

void HTMLParamElementImpl::setName(const DOMString &value)
{
    setAttribute(ATTR_NAME, value);
}

DOMString HTMLParamElementImpl::type() const
{
    return getAttribute(ATTR_TYPE);
}

void HTMLParamElementImpl::setType(const DOMString &value)
{
    setAttribute(ATTR_TYPE, value);
}

void HTMLParamElementImpl::setValue(const DOMString &value)
{
    setAttribute(ATTR_VALUE, value);
}

DOMString HTMLParamElementImpl::valueType() const
{
    return getAttribute(ATTR_VALUETYPE);
}

void HTMLParamElementImpl::setValueType(const DOMString &value)
{
    setAttribute(ATTR_VALUETYPE, value);
}

}
