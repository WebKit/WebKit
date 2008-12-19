/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#include "RenderPartObject.h"

#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLEmbedElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParamElement.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "PluginData.h"
#include "RenderView.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

RenderPartObject::RenderPartObject(HTMLFrameOwnerElement* element)
    : RenderPart(element)
{
    // init RenderObject attributes
    setInline(true);
    m_hasFallbackContent = false;
    
    if (element->hasTagName(embedTag) || element->hasTagName(objectTag))
        view()->frameView()->setIsVisuallyNonEmpty();
}

RenderPartObject::~RenderPartObject()
{
    if (m_view)
        m_view->removeWidgetToUpdate(this);
}

static bool isURLAllowed(Document* doc, const String& url)
{
    if (doc->frame()->page()->frameCount() >= 200)
        return false;

    // We allow one level of self-reference because some sites depend on that.
    // But we don't allow more than one.
    KURL completeURL = doc->completeURL(url);
    bool foundSelfReference = false;
    for (Frame* frame = doc->frame(); frame; frame = frame->tree()->parent()) {
        if (equalIgnoringRef(frame->loader()->url(), completeURL)) {
            if (foundSelfReference)
                return false;
            foundSelfReference = true;
        }
    }
    return true;
}

static inline void mapClassIdToServiceType(const String& classId, String& serviceType, const PluginData* pluginData)
{
    // Return early if classId is empty (since we won't do anything below).
    // Furthermore, if classId is null, calling get() below will crash.
    if (classId.isEmpty())
        return;

    typedef HashMap<String, String, CaseFoldingHash> ServiceTypeHashMap;
    static ServiceTypeHashMap* serviceTypeFallbackForClassId = 0;
    if (!serviceTypeFallbackForClassId) {
        serviceTypeFallbackForClassId = new ServiceTypeHashMap;
        serviceTypeFallbackForClassId->add("clsid:D27CDB6E-AE6D-11CF-96B8-444553540000", "application/x-shockwave-flash");
        serviceTypeFallbackForClassId->add("clsid:CFCDAA03-8BE4-11CF-B84B-0020AFBBCCFA", "audio/x-pn-realaudio-plugin");
        serviceTypeFallbackForClassId->add("clsid:02BF25D5-8C17-4B23-BC80-D3488ABDDC6B", "video/quicktime");
        serviceTypeFallbackForClassId->add("clsid:166B1BCA-3F9C-11CF-8075-444553540000", "application/x-director");
#if ENABLE(ACTIVEX_TYPE_CONVERSION_WMPLAYER)
        serviceTypeFallbackForClassId->add("clsid:6BF52A52-394A-11D3-B153-00C04F79FAA6", "application/x-mplayer2");
        serviceTypeFallbackForClassId->add("clsid:22D6F312-B0F6-11D0-94AB-0080C74C7E95", "application/x-mplayer2");
#endif
    }

    const String fallbackServiceType = serviceTypeFallbackForClassId->get(classId);
    if (pluginData->supportsMimeType(fallbackServiceType))
        serviceType = fallbackServiceType;
    else if (pluginData->supportsMimeType("application/x-oleobject"))
        serviceType = "application/x-oleobject";
}

static bool shouldUseChildEmbedOfObject(HTMLObjectElement* o, const PluginData* pluginData)
{
    // An OBJECT tag with a classId is some kind of ActiveX control.  The most
    // common controls have parallel plugin versions and thus possibly nested
    // EMBED tags.  If this is the case, the OBJECT's classId should map to some
    // known plugin MIME type.  If it doesn't, either the control is unlikely to
    // have a parallel plugin implementation (so there's no point looking
    // inside), or we've purposefully disabled conversion for this classId, in
    // which case we want to use the ActiveX OBJECT instead of the EMBED anyway.
    String serviceType;
    mapClassIdToServiceType(o->classId(), serviceType, pluginData);
    return serviceType != "application/x-oleobject";
}

void RenderPartObject::updateWidget(bool onlyCreateNonNetscapePlugins)
{
    String url;
    String serviceType;
    Vector<String> paramNames;
    Vector<String> paramValues;
    Frame* frame = m_view->frame();

    if (element()->hasTagName(objectTag)) {
        HTMLObjectElement* o = static_cast<HTMLObjectElement*>(element());

        o->setNeedWidgetUpdate(false);
        if (!o->isFinishedParsingChildren())
          return;

        // Check for a child EMBED tag.
        HTMLEmbedElement* embed = 0;
        const PluginData* pluginData = frame->page()->pluginData();
        if (shouldUseChildEmbedOfObject(o, pluginData)) {
            for (Node* child = o->firstChild(); child;) {
                if (child->hasTagName(embedTag)) {
                    embed = static_cast<HTMLEmbedElement*>(child);
                    break;
                } else if (child->hasTagName(objectTag))
                    child = child->nextSibling();         // Don't descend into nested OBJECT tags
                else
                    child = child->traverseNextNode(o);   // Otherwise descend (EMBEDs may be inside COMMENT tags)
            }
        }

        // Use the attributes from the EMBED tag instead of the OBJECT tag including WIDTH and HEIGHT.
        HTMLElement *embedOrObject;
        if (embed) {
            embedOrObject = (HTMLElement *)embed;
            url = embed->url();
            serviceType = embed->serviceType();
        } else
            embedOrObject = (HTMLElement *)o;

        // If there was no URL or type defined in EMBED, try the OBJECT tag.
        if (url.isEmpty())
            url = o->url();
        if (serviceType.isEmpty())
            serviceType = o->serviceType();

        HashSet<StringImpl*, CaseFoldingHash> uniqueParamNames;

        // Scan the PARAM children.
        // Get the URL and type from the params if we don't already have them.
        // Get the attributes from the params if there is no EMBED tag.
        Node *child = o->firstChild();
        while (child && (url.isEmpty() || serviceType.isEmpty() || !embed)) {
            if (child->hasTagName(paramTag)) {
                HTMLParamElement* p = static_cast<HTMLParamElement*>(child);
                String name = p->name();
                if (url.isEmpty() && (equalIgnoringCase(name, "src") || equalIgnoringCase(name, "movie") || equalIgnoringCase(name, "code") || equalIgnoringCase(name, "url")))
                    url = p->value();
                if (serviceType.isEmpty() && equalIgnoringCase(name, "type")) {
                    serviceType = p->value();
                    int pos = serviceType.find(";");
                    if (pos != -1)
                        serviceType = serviceType.left(pos);
                }
                if (!embed && !name.isEmpty()) {
                    uniqueParamNames.add(name.impl());
                    paramNames.append(p->name());
                    paramValues.append(p->value());
                }
            }
            child = child->nextSibling();
        }

        // When OBJECT is used for an applet via Sun's Java plugin, the CODEBASE attribute in the tag
        // points to the Java plugin itself (an ActiveX component) while the actual applet CODEBASE is
        // in a PARAM tag. See <http://java.sun.com/products/plugin/1.2/docs/tags.html>. This means
        // we have to explicitly suppress the tag's CODEBASE attribute if there is none in a PARAM,
        // else our Java plugin will misinterpret it. [4004531]
        String codebase;
        if (!embed && MIMETypeRegistry::isJavaAppletMIMEType(serviceType)) {
            codebase = "codebase";
            uniqueParamNames.add(codebase.impl()); // pretend we found it in a PARAM already
        }
        
        // Turn the attributes of either the EMBED tag or OBJECT tag into arrays, but don't override PARAM values.
        NamedAttrMap* attributes = embedOrObject->attributes();
        if (attributes) {
            for (unsigned i = 0; i < attributes->length(); ++i) {
                Attribute* it = attributes->attributeItem(i);
                const AtomicString& name = it->name().localName();
                if (embed || !uniqueParamNames.contains(name.impl())) {
                    paramNames.append(name.string());
                    paramValues.append(it->value().string());
                }
            }
        }

        // If we still don't have a type, try to map from a specific CLASSID to a type.
        if (serviceType.isEmpty())
            mapClassIdToServiceType(o->classId(), serviceType, pluginData);

        if (!isURLAllowed(document(), url))
            return;

        // Find out if we support fallback content.
        m_hasFallbackContent = false;
        for (Node *child = o->firstChild(); child && !m_hasFallbackContent; child = child->nextSibling()) {
            if ((!child->isTextNode() && !child->hasTagName(embedTag) && !child->hasTagName(paramTag)) || // Discount <embed> and <param>
                (child->isTextNode() && !static_cast<Text*>(child)->containsOnlyWhitespace()))
                m_hasFallbackContent = true;
        }

        if (onlyCreateNonNetscapePlugins) {
            KURL completedURL;
            if (!url.isEmpty())
                completedURL = frame->loader()->completeURL(url);

            if (frame->loader()->client()->objectContentType(completedURL, serviceType) == ObjectContentNetscapePlugin)
                return;
        }

        bool success = frame->loader()->requestObject(this, url, AtomicString(o->name()), serviceType, paramNames, paramValues);
        if (!success && m_hasFallbackContent)
            o->renderFallbackContent();
    } else if (element()->hasTagName(embedTag)) {
        HTMLEmbedElement *o = static_cast<HTMLEmbedElement*>(element());
        o->setNeedWidgetUpdate(false);
        url = o->url();
        serviceType = o->serviceType();

        if (url.isEmpty() && serviceType.isEmpty())
            return;
        if (!isURLAllowed(document(), url))
            return;

        // add all attributes set on the embed object
        NamedAttrMap* a = o->attributes();
        if (a) {
            for (unsigned i = 0; i < a->length(); ++i) {
                Attribute* it = a->attributeItem(i);
                paramNames.append(it->name().localName().string());
                paramValues.append(it->value().string());
            }
        }

        if (onlyCreateNonNetscapePlugins) {
            KURL completedURL;
            if (!url.isEmpty())
                completedURL = frame->loader()->completeURL(url);

            if (frame->loader()->client()->objectContentType(completedURL, serviceType) == ObjectContentNetscapePlugin)
                return;

        }

        frame->loader()->requestObject(this, url, o->getAttribute(nameAttr), serviceType, paramNames, paramValues);
    }
}

void RenderPartObject::layout()
{
    ASSERT(needsLayout());

    calcWidth();
    calcHeight();
    adjustOverflowForBoxShadow();

    RenderPart::layout();

    if (!m_widget && m_view)
        m_view->addWidgetToUpdate(this);

    setNeedsLayout(false);
}

void RenderPartObject::viewCleared()
{
    if (element() && m_widget && m_widget->isFrameView()) {
        FrameView* view = static_cast<FrameView*>(m_widget);
        int marginw = -1;
        int marginh = -1;
        if (element()->hasTagName(iframeTag)) {
            HTMLIFrameElement* frame = static_cast<HTMLIFrameElement*>(element());
            marginw = frame->getMarginWidth();
            marginh = frame->getMarginHeight();
        }
        if (marginw != -1)
            view->setMarginWidth(marginw);
        if (marginh != -1)
            view->setMarginHeight(marginh);
    }
}

}
