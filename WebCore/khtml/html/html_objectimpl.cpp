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
#include "html/html_documentimpl.h"
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
  : HTMLElementImpl(HTMLTags::applet(), doc)
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
    return newChild->hasTagName(HTMLTags::param()) || HTMLElementImpl::checkDTD(newChild);
}

bool HTMLAppletElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == HTMLAttributes::width() ||
        attrName == HTMLAttributes::height() ||
        attrName == HTMLAttributes::vspace() ||
        attrName == HTMLAttributes::hspace()) {
            result = eUniversal;
            return false;
    }
    
    if (attrName == HTMLAttributes::align()) {
        result = eReplaced; // Share with <img> since the alignment behavior is the same.
        return false;
    }
    
    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLAppletElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == HTMLAttributes::alt() ||
        attr->name() == HTMLAttributes::archive() ||
        attr->name() == HTMLAttributes::code() ||
        attr->name() == HTMLAttributes::codebase() ||
        attr->name() == HTMLAttributes::mayscript() ||
        attr->name() == HTMLAttributes::object()) {
        // Do nothing.
    } else if (attr->name() == HTMLAttributes::width()) {
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    } else if (attr->name() == HTMLAttributes::height()) {
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    } else if (attr->name() == HTMLAttributes::vspace()) {
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
    } else if (attr->name() == HTMLAttributes::hspace()) {
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
    } else if (attr->name() == HTMLAttributes::align()) {
        addHTMLAlignment(attr);
    } else if (attr->name() == HTMLAttributes::name()) {
        DOMString newNameAttr = attr->value();
        if (inDocument() && getDocument()->isHTMLDocument()) {
            HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
            document->removeNamedItem(oldNameAttr);
            document->addNamedItem(newNameAttr);
        }
        oldNameAttr = newNameAttr;
    } else if (attr->name() == HTMLAttributes::idAttr()) {
        DOMString newIdAttr = attr->value();
        if (inDocument() && getDocument()->isHTMLDocument()) {
            HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
            document->removeDocExtraNamedItem(oldIdAttr);
            document->addDocExtraNamedItem(newIdAttr);
        }
        oldIdAttr = newIdAttr;
        // also call superclass
        HTMLElementImpl::parseMappedAttribute(attr);
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

void HTMLAppletElementImpl::insertedIntoDocument()
{
    if (getDocument()->isHTMLDocument()) {
        HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
        document->addNamedItem(oldNameAttr);
        document->addDocExtraNamedItem(oldIdAttr);
    }

    HTMLElementImpl::insertedIntoDocument();
}

void HTMLAppletElementImpl::removedFromDocument()
{
    if (getDocument()->isHTMLDocument()) {
        HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
        document->removeNamedItem(oldNameAttr);
        document->removeDocExtraNamedItem(oldIdAttr);
    }

    HTMLElementImpl::removedFromDocument();
}

bool HTMLAppletElementImpl::rendererIsNeeded(RenderStyle *style)
{
    return !getAttribute(HTMLAttributes::code()).isNull();
}

RenderObject *HTMLAppletElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
#ifndef Q_WS_QWS // FIXME(E)? I don't think this is possible with Qt Embedded...
    KHTMLPart *part = getDocument()->part();

    if( part && part->javaEnabled() )
    {
	QMap<QString, QString> args;

	args.insert( "code", getAttribute(HTMLAttributes::code()).string());
	DOMString codeBase = getAttribute(HTMLAttributes::codebase());
	if(!codeBase.isNull())
	    args.insert( "codeBase", codeBase.string() );
	DOMString name = getDocument()->htmlMode() != DocumentImpl::XHtml ?
			 getAttribute(HTMLAttributes::name()) : getAttribute(HTMLAttributes::idAttr());
	if(!name.isNull())
	    args.insert( "name", name.string() );
	DOMString archive = getAttribute(HTMLAttributes::archive());
	if(!archive.isNull())
	    args.insert( "archive", archive.string() );

	args.insert( "baseURL", getDocument()->baseURL() );

        DOMString mayScript = getAttribute(HTMLAttributes::mayscript());
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
    return getAttribute(HTMLAttributes::align());
}

void HTMLAppletElementImpl::setAlign(const DOMString &value)
{
    setAttribute(HTMLAttributes::align(), value);
}

DOMString HTMLAppletElementImpl::alt() const
{
    return getAttribute(HTMLAttributes::alt());
}

void HTMLAppletElementImpl::setAlt(const DOMString &value)
{
    setAttribute(HTMLAttributes::alt(), value);
}

DOMString HTMLAppletElementImpl::archive() const
{
    return getAttribute(HTMLAttributes::archive());
}

void HTMLAppletElementImpl::setArchive(const DOMString &value)
{
    setAttribute(HTMLAttributes::archive(), value);
}

DOMString HTMLAppletElementImpl::code() const
{
    return getAttribute(HTMLAttributes::code());
}

void HTMLAppletElementImpl::setCode(const DOMString &value)
{
    setAttribute(HTMLAttributes::code(), value);
}

DOMString HTMLAppletElementImpl::codeBase() const
{
    return getAttribute(HTMLAttributes::codebase());
}

void HTMLAppletElementImpl::setCodeBase(const DOMString &value)
{
    setAttribute(HTMLAttributes::codebase(), value);
}

DOMString HTMLAppletElementImpl::height() const
{
    return getAttribute(HTMLAttributes::height());
}

void HTMLAppletElementImpl::setHeight(const DOMString &value)
{
    setAttribute(HTMLAttributes::height(), value);
}

DOMString HTMLAppletElementImpl::hspace() const
{
    return getAttribute(HTMLAttributes::hspace());
}

void HTMLAppletElementImpl::setHspace(const DOMString &value)
{
    setAttribute(HTMLAttributes::hspace(), value);
}

DOMString HTMLAppletElementImpl::name() const
{
    return getAttribute(HTMLAttributes::name());
}

void HTMLAppletElementImpl::setName(const DOMString &value)
{
    setAttribute(HTMLAttributes::name(), value);
}

DOMString HTMLAppletElementImpl::object() const
{
    return getAttribute(HTMLAttributes::object());
}

void HTMLAppletElementImpl::setObject(const DOMString &value)
{
    setAttribute(HTMLAttributes::object(), value);
}

DOMString HTMLAppletElementImpl::vspace() const
{
    return getAttribute(HTMLAttributes::vspace());
}

void HTMLAppletElementImpl::setVspace(const DOMString &value)
{
    setAttribute(HTMLAttributes::vspace(), value);
}

DOMString HTMLAppletElementImpl::width() const
{
    return getAttribute(HTMLAttributes::width());
}

void HTMLAppletElementImpl::setWidth(const DOMString &value)
{
    setAttribute(HTMLAttributes::width(), value);
}

// -------------------------------------------------------------------------

HTMLEmbedElementImpl::HTMLEmbedElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(HTMLTags::embed(), doc), embedInstance(0)
{}

HTMLEmbedElementImpl::~HTMLEmbedElementImpl()
{
}

bool HTMLEmbedElementImpl::checkDTD(const NodeImpl* newChild)
{
    return newChild->hasTagName(HTMLTags::param()) || HTMLElementImpl::checkDTD(newChild);
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

bool HTMLEmbedElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == HTMLAttributes::width() ||
        attrName == HTMLAttributes::height() ||
        attrName == HTMLAttributes::border() ||
        attrName == HTMLAttributes::vspace() ||
        attrName == HTMLAttributes::hspace() ||
        attrName == HTMLAttributes::valign() ||
        attrName == HTMLAttributes::hidden()) {
        result = eUniversal;
        return false;
    }
        
    if (attrName == HTMLAttributes::align()) {
        result = eReplaced; // Share with <img> since the alignment behavior is the same.
        return false;
    }
    
    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLEmbedElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    QString val = attr->value().string();
  
    int pos;
    if (attr->name() == HTMLAttributes::type()) {
        serviceType = val.lower();
        pos = serviceType.find( ";" );
        if ( pos!=-1 )
            serviceType = serviceType.left( pos );
    } else if (attr->name() == HTMLAttributes::code() ||
               attr->name() == HTMLAttributes::src()) {
         url = khtml::parseURL(attr->value()).string();
    } else if (attr->name() == HTMLAttributes::width()) {
        addCSSLength( attr, CSS_PROP_WIDTH, attr->value() );
    } else if (attr->name() == HTMLAttributes::height()) {
        addCSSLength( attr, CSS_PROP_HEIGHT, attr->value());
    } else if (attr->name() == HTMLAttributes::border()) {
        addCSSLength(attr, CSS_PROP_BORDER_WIDTH, attr->value());
        addCSSProperty( attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID );
        addCSSProperty( attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID );
        addCSSProperty( attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID );
        addCSSProperty( attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID );
    } else if (attr->name() == HTMLAttributes::vspace()) {
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
    } else if (attr->name() == HTMLAttributes::hspace()) {
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
    } else if (attr->name() == HTMLAttributes::align()) {
	addHTMLAlignment(attr);
    } else if (attr->name() == HTMLAttributes::valign()) {
        addCSSProperty(attr, CSS_PROP_VERTICAL_ALIGN, attr->value());
    } else if (attr->name() == HTMLAttributes::pluginpage() ||
               attr->name() == HTMLAttributes::pluginspage()) {
        pluginPage = val;
    } else if (attr->name() == HTMLAttributes::hidden()) {
        if (val.lower()=="yes" || val.lower()=="true") {
            // FIXME: Not dynamic, but it's not really important that such a rarely-used
            // feature work dynamically.
            addCSSLength( attr, CSS_PROP_WIDTH, "0" );
            addCSSLength( attr, CSS_PROP_HEIGHT, "0" );
        }
    } else if (attr->name() == HTMLAttributes::name()) {
        DOMString newNameAttr = attr->value();
        if (inDocument() && getDocument()->isHTMLDocument()) {
            HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
            document->removeNamedItem(oldNameAttr);
            document->addNamedItem(newNameAttr);
        }
        oldNameAttr = newNameAttr;
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

bool HTMLEmbedElementImpl::rendererIsNeeded(RenderStyle *style)
{
    KHTMLPart *part = getDocument()->part();
    if (!part)
	return false;
    return part->pluginsEnabled() && !parentNode()->hasTagName(HTMLTags::object());
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

void HTMLEmbedElementImpl::insertedIntoDocument()
{
    if (getDocument()->isHTMLDocument()) {
        HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
        document->addNamedItem(oldNameAttr);
    }

    HTMLElementImpl::insertedIntoDocument();
}

void HTMLEmbedElementImpl::removedFromDocument()
{
    if (getDocument()->isHTMLDocument()) {
        HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
        document->removeNamedItem(oldNameAttr);
    }

    HTMLElementImpl::removedFromDocument();
}

bool HTMLEmbedElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->name() == HTMLAttributes::src();
}

// -------------------------------------------------------------------------

HTMLObjectElementImpl::HTMLObjectElementImpl(DocumentPtr *doc) 
#if APPLE_CHANGES
: HTMLElementImpl(HTMLTags::object(), doc), m_imageLoader(0), objectInstance(0)
#else
: HTMLElementImpl(HTMLTags::object(), doc), m_imageLoader(0)
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
    return newChild->hasTagName(HTMLTags::param()) || HTMLElementImpl::checkDTD(newChild);
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
        if (p->hasTagName(HTMLTags::form()))
            return static_cast<HTMLFormElementImpl *>(p);
    }
    
    return 0;
}

bool HTMLObjectElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == HTMLAttributes::width() ||
        attrName == HTMLAttributes::height() ||
        attrName == HTMLAttributes::vspace() ||
        attrName == HTMLAttributes::hspace()) {
        result = eUniversal;
        return false;
    }
    
    if (attrName == HTMLAttributes::align()) {
        result = eReplaced; // Share with <img> since the alignment behavior is the same.
        return false;
    }
    
    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLObjectElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    QString val = attr->value().string();
    int pos;
    if (attr->name() == HTMLAttributes::type()) {
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
    } else if (attr->name() == HTMLAttributes::data()) {
        url = khtml::parseURL(  val ).string();
        if (m_render)
          needWidgetUpdate = true;
        if (m_render && isImageType()) {
          if (!m_imageLoader)
              m_imageLoader = new HTMLImageLoader(this);
          m_imageLoader->updateFromElement();
        }
    } else if (attr->name() == HTMLAttributes::width()) {
        addCSSLength( attr, CSS_PROP_WIDTH, attr->value());
    } else if (attr->name() == HTMLAttributes::height()) {
        addCSSLength( attr, CSS_PROP_HEIGHT, attr->value());
    } else if (attr->name() == HTMLAttributes::vspace()) {
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
    } else if (attr->name() == HTMLAttributes::hspace()) {
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
    } else if (attr->name() == HTMLAttributes::align()) {
        addHTMLAlignment(attr);
    } else if (attr->name() == HTMLAttributes::classid()) {
        classId = val;
        if (m_render)
          needWidgetUpdate = true;
    } else if (attr->name() == HTMLAttributes::onload()) {
        setHTMLEventListener(EventImpl::LOAD_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onunload()) {
        setHTMLEventListener(EventImpl::UNLOAD_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::name()) {
	    DOMString newNameAttr = attr->value();
	    if (inDocument() && getDocument()->isHTMLDocument()) {
		HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
		document->removeNamedItem(oldNameAttr);
		document->addNamedItem(newNameAttr);
	    }
	    oldNameAttr = newNameAttr;
    } else if (attr->name() == HTMLAttributes::idAttr()) {
        DOMString newIdAttr = attr->value();
        if (inDocument() && getDocument()->isHTMLDocument()) {
            HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
            document->removeDocExtraNamedItem(oldIdAttr);
            document->addDocExtraNamedItem(newIdAttr);
        }
        oldIdAttr = newIdAttr;
        // also call superclass
        HTMLElementImpl::parseMappedAttribute(attr);
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
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

void HTMLObjectElementImpl::insertedIntoDocument()
{
    if (getDocument()->isHTMLDocument()) {
        HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
        document->addNamedItem(oldNameAttr);
        document->addDocExtraNamedItem(oldIdAttr);
    }

    HTMLElementImpl::insertedIntoDocument();
}

void HTMLObjectElementImpl::removedFromDocument()
{
    if (getDocument()->isHTMLDocument()) {
        HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
        document->removeNamedItem(oldNameAttr);
        document->removeDocExtraNamedItem(oldIdAttr);
    }

    HTMLElementImpl::removedFromDocument();
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
    return (attr->name() == HTMLAttributes::data() || (attr->name() == HTMLAttributes::usemap() && attr->value().domString()[0] != '#'));
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
    return getAttribute(HTMLAttributes::code());
}

void HTMLObjectElementImpl::setCode(const DOMString &value)
{
    setAttribute(HTMLAttributes::code(), value);
}

DOMString HTMLObjectElementImpl::align() const
{
    return getAttribute(HTMLAttributes::align());
}

void HTMLObjectElementImpl::setAlign(const DOMString &value)
{
    setAttribute(HTMLAttributes::align(), value);
}

DOMString HTMLObjectElementImpl::archive() const
{
    return getAttribute(HTMLAttributes::archive());
}

void HTMLObjectElementImpl::setArchive(const DOMString &value)
{
    setAttribute(HTMLAttributes::archive(), value);
}

DOMString HTMLObjectElementImpl::border() const
{
    return getAttribute(HTMLAttributes::border());
}

void HTMLObjectElementImpl::setBorder(const DOMString &value)
{
    setAttribute(HTMLAttributes::border(), value);
}

DOMString HTMLObjectElementImpl::codeBase() const
{
    return getAttribute(HTMLAttributes::codebase());
}

void HTMLObjectElementImpl::setCodeBase(const DOMString &value)
{
    setAttribute(HTMLAttributes::codebase(), value);
}

DOMString HTMLObjectElementImpl::codeType() const
{
    return getAttribute(HTMLAttributes::codetype());
}

void HTMLObjectElementImpl::setCodeType(const DOMString &value)
{
    setAttribute(HTMLAttributes::codetype(), value);
}

DOMString HTMLObjectElementImpl::data() const
{
    return getAttribute(HTMLAttributes::data());
}

void HTMLObjectElementImpl::setData(const DOMString &value)
{
    setAttribute(HTMLAttributes::data(), value);
}

bool HTMLObjectElementImpl::declare() const
{
    return !getAttribute(HTMLAttributes::declare()).isNull();
}

void HTMLObjectElementImpl::setDeclare(bool declare)
{
    setAttribute(HTMLAttributes::declare(), declare ? "" : 0);
}

DOMString HTMLObjectElementImpl::height() const
{
    return getAttribute(HTMLAttributes::height());
}

void HTMLObjectElementImpl::setHeight(const DOMString &value)
{
    setAttribute(HTMLAttributes::height(), value);
}

DOMString HTMLObjectElementImpl::hspace() const
{
    return getAttribute(HTMLAttributes::hspace());
}

void HTMLObjectElementImpl::setHspace(const DOMString &value)
{
    setAttribute(HTMLAttributes::hspace(), value);
}

DOMString HTMLObjectElementImpl::name() const
{
    return getAttribute(HTMLAttributes::name());
}

void HTMLObjectElementImpl::setName(const DOMString &value)
{
    setAttribute(HTMLAttributes::name(), value);
}

DOMString HTMLObjectElementImpl::standby() const
{
    return getAttribute(HTMLAttributes::standby());
}

void HTMLObjectElementImpl::setStandby(const DOMString &value)
{
    setAttribute(HTMLAttributes::standby(), value);
}

long HTMLObjectElementImpl::tabIndex() const
{
    return getAttribute(HTMLAttributes::tabindex()).toInt();
}

void HTMLObjectElementImpl::setTabIndex(long tabIndex)
{
    setAttribute(HTMLAttributes::tabindex(), QString::number(tabIndex));
}

DOMString HTMLObjectElementImpl::type() const
{
    return getAttribute(HTMLAttributes::type());
}

void HTMLObjectElementImpl::setType(const DOMString &value)
{
    setAttribute(HTMLAttributes::type(), value);
}

DOMString HTMLObjectElementImpl::useMap() const
{
    return getAttribute(HTMLAttributes::usemap());
}

void HTMLObjectElementImpl::setUseMap(const DOMString &value)
{
    setAttribute(HTMLAttributes::usemap(), value);
}

DOMString HTMLObjectElementImpl::vspace() const
{
    return getAttribute(HTMLAttributes::vspace());
}

void HTMLObjectElementImpl::setVspace(const DOMString &value)
{
    setAttribute(HTMLAttributes::vspace(), value);
}

DOMString HTMLObjectElementImpl::width() const
{
    return getAttribute(HTMLAttributes::width());
}

void HTMLObjectElementImpl::setWidth(const DOMString &value)
{
    setAttribute(HTMLAttributes::width(), value);
}

// -------------------------------------------------------------------------

HTMLParamElementImpl::HTMLParamElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(HTMLTags::param(), doc)
{
}

HTMLParamElementImpl::~HTMLParamElementImpl()
{
}

void HTMLParamElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == HTMLAttributes::idAttr()) {
        // Must call base class so that hasID bit gets set.
        HTMLElementImpl::parseMappedAttribute(attr);
        if (getDocument()->htmlMode() != DocumentImpl::XHtml)
            return;
        m_name = attr->value();
    } else if (attr->name() == HTMLAttributes::name()) {
        m_name = attr->value();
    } else if (attr->name() == HTMLAttributes::value()) {
        m_value = attr->value();
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

bool HTMLParamElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    if (attr->name() == HTMLAttributes::value()) {
        AttributeImpl *attr = attributes()->getAttributeItem(HTMLAttributes::name());
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
    setAttribute(HTMLAttributes::name(), value);
}

DOMString HTMLParamElementImpl::type() const
{
    return getAttribute(HTMLAttributes::type());
}

void HTMLParamElementImpl::setType(const DOMString &value)
{
    setAttribute(HTMLAttributes::type(), value);
}

void HTMLParamElementImpl::setValue(const DOMString &value)
{
    setAttribute(HTMLAttributes::value(), value);
}

DOMString HTMLParamElementImpl::valueType() const
{
    return getAttribute(HTMLAttributes::valuetype());
}

void HTMLParamElementImpl::setValueType(const DOMString &value)
{
    setAttribute(HTMLAttributes::valuetype(), value);
}

}
