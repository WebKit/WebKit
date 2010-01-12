/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "RenderEmbeddedObject.h"

#include "Frame.h"
#include "FrameLoaderClient.h"
#include "HTMLEmbedElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParamElement.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "PluginWidget.h"
#include "RenderView.h"
#include "RenderWidgetProtector.h"
#include "Text.h"

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "HTMLVideoElement.h"
#endif

#if USE(ACCELERATED_COMPOSITING)
#include "PluginWidget.h"
#endif

namespace WebCore {

using namespace HTMLNames;

RenderEmbeddedObject::RenderEmbeddedObject(Element* element)
    : RenderPartObject(element)
{
    view()->frameView()->setIsVisuallyNonEmpty();
}

RenderEmbeddedObject::~RenderEmbeddedObject()
{
    if (frameView())
        frameView()->removeWidgetToUpdate(this);
}

#if USE(ACCELERATED_COMPOSITING)
bool RenderEmbeddedObject::requiresLayer() const
{
    if (RenderPartObject::requiresLayer())
        return true;
    
    return allowsAcceleratedCompositing();
}

bool RenderEmbeddedObject::allowsAcceleratedCompositing() const
{
    return widget() && widget()->isPluginWidget() && static_cast<PluginWidget*>(widget())->platformLayer();
}
#endif

static bool isURLAllowed(Document* doc, const String& url)
{
    if (doc->frame()->page()->frameCount() >= 200)
        return false;

    // We allow one level of self-reference because some sites depend on that.
    // But we don't allow more than one.
    KURL completeURL = doc->completeURL(url);
    bool foundSelfReference = false;
    for (Frame* frame = doc->frame(); frame; frame = frame->tree()->parent()) {
        if (equalIgnoringFragmentIdentifier(frame->loader()->url(), completeURL)) {
            if (foundSelfReference)
                return false;
            foundSelfReference = true;
        }
    }
    return true;
}

typedef HashMap<String, String, CaseFoldingHash> ClassIdToTypeMap;

static ClassIdToTypeMap* createClassIdToTypeMap()
{
    ClassIdToTypeMap* map = new ClassIdToTypeMap;
    map->add("clsid:D27CDB6E-AE6D-11CF-96B8-444553540000", "application/x-shockwave-flash");
    map->add("clsid:CFCDAA03-8BE4-11CF-B84B-0020AFBBCCFA", "audio/x-pn-realaudio-plugin");
    map->add("clsid:02BF25D5-8C17-4B23-BC80-D3488ABDDC6B", "video/quicktime");
    map->add("clsid:166B1BCA-3F9C-11CF-8075-444553540000", "application/x-director");
    map->add("clsid:6BF52A52-394A-11D3-B153-00C04F79FAA6", "application/x-mplayer2");
    map->add("clsid:22D6F312-B0F6-11D0-94AB-0080C74C7E95", "application/x-mplayer2");
    return map;
}

static String serviceTypeForClassId(const String& classId)
{
    // Return early if classId is empty (since we won't do anything below).
    // Furthermore, if classId is null, calling get() below will crash.
    if (classId.isEmpty())
        return String();

    static ClassIdToTypeMap* map = createClassIdToTypeMap();
    return map->get(classId);
}

static void mapDataParamToSrc(Vector<String>* paramNames, Vector<String>* paramValues)
{
    // Some plugins don't understand the "data" attribute of the OBJECT tag (i.e. Real and WMP
    // require "src" attribute).
    int srcIndex = -1, dataIndex = -1;
    for (unsigned int i = 0; i < paramNames->size(); ++i) {
        if (equalIgnoringCase((*paramNames)[i], "src"))
            srcIndex = i;
        else if (equalIgnoringCase((*paramNames)[i], "data"))
            dataIndex = i;
    }

    if (srcIndex == -1 && dataIndex != -1) {
        paramNames->append("src");
        paramValues->append((*paramValues)[dataIndex]);
    }
}

void RenderEmbeddedObject::updateWidget(bool onlyCreateNonNetscapePlugins)
{
    String url;
    String serviceType;
    Vector<String> paramNames;
    Vector<String> paramValues;
    Frame* frame = frameView()->frame();

    // The calls to FrameLoader::requestObject within this function can result in a plug-in being initialized.
    // This can run cause arbitrary JavaScript to run and may result in this RenderObject being detached from
    // the render tree and destroyed, causing a crash like <rdar://problem/6954546>.  By extending our lifetime
    // artifically to ensure that we remain alive for the duration of plug-in initialization.
    RenderWidgetProtector protector(this);

    if (node()->hasTagName(objectTag)) {
        HTMLObjectElement* objectElement = static_cast<HTMLObjectElement*>(node());

        objectElement->setNeedWidgetUpdate(false);
        if (!objectElement->isFinishedParsingChildren())
          return;

        // Check for a child EMBED tag.
        HTMLEmbedElement* embed = 0;
        for (Node* child = objectElement->firstChild(); child; ) {
            if (child->hasTagName(embedTag)) {
                embed = static_cast<HTMLEmbedElement*>(child);
                break;
            }
            
            if (child->hasTagName(objectTag))
                child = child->nextSibling(); // Don't descend into nested OBJECT tags
            else
                child = child->traverseNextNode(objectElement); // Otherwise descend (EMBEDs may be inside COMMENT tags)
        }

        // Use the attributes from the EMBED tag instead of the OBJECT tag including WIDTH and HEIGHT.
        HTMLElement* embedOrObject;
        if (embed) {
            embedOrObject = embed;
            url = embed->url();
            serviceType = embed->serviceType();
        } else
            embedOrObject = objectElement;

        // If there was no URL or type defined in EMBED, try the OBJECT tag.
        if (url.isEmpty())
            url = objectElement->url();
        if (serviceType.isEmpty())
            serviceType = objectElement->serviceType();

        HashSet<StringImpl*, CaseFoldingHash> uniqueParamNames;

        // Scan the PARAM children.
        // Get the URL and type from the params if we don't already have them.
        // Get the attributes from the params if there is no EMBED tag.
        Node* child = objectElement->firstChild();
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
        NamedNodeMap* attributes = embedOrObject->attributes();
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

        mapDataParamToSrc(&paramNames, &paramValues);

        // If we still don't have a type, try to map from a specific CLASSID to a type.
        if (serviceType.isEmpty())
            serviceType = serviceTypeForClassId(objectElement->classId());

        if (!isURLAllowed(document(), url))
            return;

        // Find out if we support fallback content.
        m_hasFallbackContent = false;
        for (Node* child = objectElement->firstChild(); child && !m_hasFallbackContent; child = child->nextSibling()) {
            if ((!child->isTextNode() && !child->hasTagName(embedTag) && !child->hasTagName(paramTag))  // Discount <embed> and <param>
                || (child->isTextNode() && !static_cast<Text*>(child)->containsOnlyWhitespace()))
                m_hasFallbackContent = true;
        }

        if (onlyCreateNonNetscapePlugins) {
            KURL completedURL;
            if (!url.isEmpty())
                completedURL = frame->loader()->completeURL(url);

            if (frame->loader()->client()->objectContentType(completedURL, serviceType) == ObjectContentNetscapePlugin)
                return;
        }

        bool success = objectElement->dispatchBeforeLoadEvent(url) && frame->loader()->requestObject(this, url, objectElement->getAttribute(nameAttr), serviceType, paramNames, paramValues);
        if (!success && m_hasFallbackContent)
            objectElement->renderFallbackContent();

    } else if (node()->hasTagName(embedTag)) {
        HTMLEmbedElement* embedElement = static_cast<HTMLEmbedElement*>(node());
        embedElement->setNeedWidgetUpdate(false);
        url = embedElement->url();
        serviceType = embedElement->serviceType();

        if (url.isEmpty() && serviceType.isEmpty())
            return;
        if (!isURLAllowed(document(), url))
            return;

        // add all attributes set on the embed object
        NamedNodeMap* attributes = embedElement->attributes();
        if (attributes) {
            for (unsigned i = 0; i < attributes->length(); ++i) {
                Attribute* it = attributes->attributeItem(i);
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

        if (embedElement->dispatchBeforeLoadEvent(url))
            frame->loader()->requestObject(this, url, embedElement->getAttribute(nameAttr), serviceType, paramNames, paramValues);
    }
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)        
    else if (node()->hasTagName(videoTag) || node()->hasTagName(audioTag)) {
        HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(node());

        mediaElement->setNeedWidgetUpdate(false);
        if (node()->hasTagName(videoTag)) {
            HTMLVideoElement* vid = static_cast<HTMLVideoElement*>(node());
            String poster = vid->poster();
            if (!poster.isEmpty()) {
                paramNames.append("_media_element_poster_");
                paramValues.append(poster);
            }
        }

        url = mediaElement->initialURL();
        if (!url.isEmpty()) {
            paramNames.append("_media_element_src_");
            paramValues.append(url);
        }

        serviceType = "application/x-media-element-proxy-plugin";
        
        if (mediaElement->dispatchBeforeLoadEvent(url))
            frame->loader()->requestObject(this, url, nullAtom, serviceType, paramNames, paramValues);
    }
#endif
}

void RenderEmbeddedObject::layout()
{
    ASSERT(needsLayout());

    calcWidth();
    calcHeight();

    RenderPart::layout();

    m_overflow.clear();
    addShadowOverflow();

    if (!widget() && frameView())
        frameView()->addWidgetToUpdate(this);

    setNeedsLayout(false);
}

}
