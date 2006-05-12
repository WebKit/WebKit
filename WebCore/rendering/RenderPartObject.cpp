/**
 * This file is part of the KDE project.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "RenderPartObject.h"

#include "Document.h"
#include "Frame.h"
#include "FrameTree.h"
#include "HTMLNames.h"
#include "HTMLEmbedElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLObjectElement.h"
#include "HTMLParamElement.h"
#include "Page.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

RenderPartObject::RenderPartObject(HTMLElement* element)
    : RenderPart(element)
{
    // init RenderObject attributes
    setInline(true);
    m_hasFallbackContent = false;
}

static bool isURLAllowed(Document *doc, const String &url)
{
    KURL newURL(doc->completeURL(url.deprecatedString()));
    newURL.setRef(DeprecatedString::null);
    
    if (doc->frame()->page()->frameCount() >= 200)
        return false;

    // We allow one level of self-reference because some sites depend on that.
    // But we don't allow more than one.
    bool foundSelfReference = false;
    for (Frame *frame = doc->frame(); frame; frame = frame->tree()->parent()) {
        KURL frameURL = frame->url();
        frameURL.setRef(DeprecatedString::null);
        if (frameURL == newURL) {
            if (foundSelfReference)
                return false;
            foundSelfReference = true;
        }
    }
    return true;
}

static inline void mapClassIdToServiceType(const String& classId, String& serviceType)
{
    // It is ActiveX, but the nsplugin system handling
    // should also work, that's why we don't override the
    // serviceType with application/x-activex-handler
    // but let the KTrader in khtmlpart::createPart() detect
    // the user's preference: launch with activex viewer or
    // with nspluginviewer (Niko)
    if (classId.contains("D27CDB6E-AE6D-11cf-96B8-444553540000"))
        serviceType = "application/x-shockwave-flash";
    else if (classId.contains("CFCDAA03-8BE4-11cf-B84B-0020AFBBCCFA"))
        serviceType = "audio/x-pn-realaudio-plugin";
    else if (classId.contains("02BF25D5-8C17-4B23-BC80-D3488ABDDC6B"))
        serviceType = "video/quicktime";
    else if (classId.contains("166B1BCA-3F9C-11CF-8075-444553540000"))
        serviceType = "application/x-director";
    else if (classId.contains("6BF52A52-394A-11d3-B153-00C04F79FAA6"))
        serviceType = "application/x-mplayer2";
    else if (!classId.isEmpty())
        // We have a clsid, means this is activex (Niko)
        serviceType = "application/x-activex-handler";
    // TODO: add more plugins here
}

void RenderPartObject::updateWidget()
{
  String url;
  String serviceType;
  Vector<String> paramNames;
  Vector<String> paramValues;
  Frame* frame = m_view->frame();

  setNeedsLayoutAndMinMaxRecalc();

  if (element()->hasTagName(objectTag)) {

      HTMLObjectElement* o = static_cast<HTMLObjectElement*>(element());

      if (!o->isComplete())
        return;
      // Check for a child EMBED tag.
      HTMLEmbedElement* embed = 0;
      for (Node* child = o->firstChild(); child;) {
          if (child->hasTagName(embedTag)) {
              embed = static_cast<HTMLEmbedElement*>(child);
              break;
          } else if (child->hasTagName(objectTag))
              child = child->nextSibling();         // Don't descend into nested OBJECT tags
          else
              child = child->traverseNextNode(o);   // Otherwise descend (EMBEDs may be inside COMMENT tags)
      }
      
      // Use the attributes from the EMBED tag instead of the OBJECT tag including WIDTH and HEIGHT.
      HTMLElement *embedOrObject;
      if (embed) {
          embedOrObject = (HTMLElement *)embed;
          String attribute = embedOrObject->getAttribute(widthAttr);
          if (!attribute.isEmpty())
              o->setAttribute(widthAttr, attribute);
          attribute = embedOrObject->getAttribute(heightAttr);
          if (!attribute.isEmpty())
              o->setAttribute(heightAttr, attribute);
          url = embed->url;
          serviceType = embed->serviceType;
      } else
          embedOrObject = (HTMLElement *)o;
      
      // If there was no URL or type defined in EMBED, try the OBJECT tag.
      if (url.isEmpty())
          url = o->url;
      if (serviceType.isEmpty())
          serviceType = o->serviceType;
      
      HashSet<StringImpl*, CaseInsensitiveHash> uniqueParamNames;
      
      // Scan the PARAM children.
      // Get the URL and type from the params if we don't already have them.
      // Get the attributes from the params if there is no EMBED tag.
      Node *child = o->firstChild();
      while (child && (url.isEmpty() || serviceType.isEmpty() || !embed)) {
          if (child->hasTagName(paramTag)) {
              HTMLParamElement* p = static_cast<HTMLParamElement*>(child);
              String name = p->name().lower();
              if (url.isEmpty() && (name == "src" || name == "movie" || name == "code" || name == "url"))
                  url = p->value();
              if (serviceType.isEmpty() && name == "type") {
                  serviceType = p->value();
                  int pos = serviceType.find(";");
                  if (pos != -1)
                      serviceType = serviceType.left(pos);
              }
              if (!embed && !name.isEmpty()) {
                  uniqueParamNames.add(p->name().impl());
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
      if (!embed && serviceType.lower() == "application/x-java-applet") {
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
                  paramNames.append(name.domString());
                  paramValues.append(it->value().domString());
              }
          }
      }
      
      // If we still don't have a type, try to map from a specific CLASSID to a type.
      if (serviceType.isEmpty() && !o->classId.isEmpty())
          mapClassIdToServiceType(o->classId, serviceType);
      
      // If no URL and type, abort.
      if (url.isEmpty() && serviceType.isEmpty())
          return;
      if (!isURLAllowed(document(), url))
          return;

      // Find out if we support fallback content.
      m_hasFallbackContent = false;
      for (Node *child = o->firstChild(); child && !m_hasFallbackContent; child = child->nextSibling()) {
          if ((!child->isTextNode() && !child->hasTagName(embedTag) && !child->hasTagName(paramTag)) || // Discount <embed> and <param>
              (child->isTextNode() && !static_cast<Text*>(child)->containsOnlyWhitespace()))
              m_hasFallbackContent = true;
      }
      bool success = frame->requestObject(this, url, AtomicString(o->name()), serviceType, paramNames, paramValues);
      if (!success && m_hasFallbackContent)
          o->renderFallbackContent();
  } else if (element()->hasTagName(embedTag)) {
      HTMLEmbedElement *o = static_cast<HTMLEmbedElement*>(element());
      url = o->url;
      serviceType = o->serviceType;

      if (url.isEmpty() && serviceType.isEmpty())
          return;
      if (!isURLAllowed(document(), url))
          return;
      
      // add all attributes set on the embed object
      NamedAttrMap* a = o->attributes();
      if (a) {
          for (unsigned i = 0; i < a->length(); ++i) {
              Attribute* it = a->attributeItem(i);
              paramNames.append(it->name().localName().domString());
              paramValues.append(it->value().domString());
          }
      }
      frame->requestObject(this, url, o->getAttribute(nameAttr), serviceType, paramNames, paramValues);
  } else {
      assert(element()->hasTagName(iframeTag));
      HTMLIFrameElement* o = static_cast<HTMLIFrameElement*>(element());
      url = o->m_URL;
      if (!isURLAllowed(document(), url))
          return;
      if (url.isEmpty())
          url = "about:blank";
      FrameView* v = static_cast<FrameView*>(m_view);
      v->frame()->requestFrame(this, url, o->m_name);
  }
}

void RenderPartObject::layout()
{
    KHTMLAssert(needsLayout());
    KHTMLAssert(minMaxKnown());

    calcWidth();
    calcHeight();

    RenderPart::layout();

    setNeedsLayout(false);
}

void RenderPartObject::viewCleared()
{
    if (element() && m_widget && m_widget->isFrameView()) {
        FrameView* view = static_cast<FrameView*>(m_widget);
        bool hasBorder = false;
        int marginw = -1;
        int marginh = -1;
        if (element()->hasTagName(iframeTag)) {
            HTMLIFrameElement* frame = static_cast<HTMLIFrameElement*>(element());
            hasBorder = frame->m_frameBorder;
            marginw = frame->m_marginWidth;
            marginh = frame->m_marginHeight;
        }

        view->setHasBorder(hasBorder);
        view->setIgnoreWheelEvents(element()->hasTagName(iframeTag));
        if (marginw != -1)
            view->setMarginWidth(marginw);
        if (marginh != -1)
            view->setMarginHeight(marginh);
    }
}

}
