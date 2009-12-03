/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorFrontendHost.h"

#if ENABLE(INSPECTOR)

#include "Element.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLFrameOwnerElement.h"
#include "InspectorClient.h"
#include "InspectorResource.h"

#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>

using namespace std;

namespace WebCore {

InspectorFrontendHost::InspectorFrontendHost(InspectorController* inspectorController, InspectorClient* client)
    : m_inspectorController(inspectorController)
    , m_client(client)
{
}

InspectorFrontendHost::~InspectorFrontendHost()
{
}

void InspectorFrontendHost::loaded()
{
    if (m_inspectorController)
        m_inspectorController->scriptObjectReady();
}

void InspectorFrontendHost::attach()
{
    if (m_inspectorController)
        m_inspectorController->attachWindow();
}

void InspectorFrontendHost::detach()
{
    if (m_inspectorController)
        m_inspectorController->detachWindow();
}

void InspectorFrontendHost::closeWindow()
{
    if (m_inspectorController)
        m_inspectorController->closeWindow();
}

void InspectorFrontendHost::windowUnloading()
{
    if (m_inspectorController)
        m_inspectorController->close();
}

void InspectorFrontendHost::setAttachedWindowHeight(unsigned height)
{
    if (m_inspectorController)
        m_inspectorController->setAttachedWindowHeight(height);
}

void InspectorFrontendHost::moveWindowBy(float x, float y) const
{
    if (m_inspectorController)
        m_inspectorController->moveWindowBy(x, y);
}

String InspectorFrontendHost::localizedStringsURL()
{
    return m_client->localizedStringsURL();
}

String InspectorFrontendHost::hiddenPanels()
{
    return m_client->hiddenPanels();
}

const String& InspectorFrontendHost::platform() const
{
#if PLATFORM(MAC)
#ifdef BUILDING_ON_TIGER
    DEFINE_STATIC_LOCAL(const String, platform, ("mac-tiger"));
#else
    DEFINE_STATIC_LOCAL(const String, platform, ("mac-leopard"));
#endif
#elif PLATFORM(WIN_OS)
    DEFINE_STATIC_LOCAL(const String, platform, ("windows"));
#else
    DEFINE_STATIC_LOCAL(const String, platform, ("unknown"));
#endif
    return platform;
}


const String& InspectorFrontendHost::port() const
{
#if PLATFORM(QT)
    DEFINE_STATIC_LOCAL(const String, port, ("qt"));
#elif PLATFORM(GTK)
    DEFINE_STATIC_LOCAL(const String, port, ("gtk"));
#elif PLATFORM(WX)
    DEFINE_STATIC_LOCAL(const String, port, ("wx"));
#else
    DEFINE_STATIC_LOCAL(const String, port, ("unknown"));
#endif

    return port;
}

void InspectorFrontendHost::addResourceSourceToFrame(long identifier, Node* frame)
{
    if (!m_inspectorController)
        return;
    RefPtr<InspectorResource> resource = m_inspectorController->resources().get(identifier);
    if (resource) {
        String sourceString = resource->sourceString();
        if (!sourceString.isEmpty())
            addSourceToFrame(resource->mimeType(), sourceString, frame);
    }
}

bool InspectorFrontendHost::addSourceToFrame(const String& mimeType, const String& source, Node* frameNode)
{
    ASSERT_ARG(frameNode, frameNode);

    if (!frameNode)
        return false;

    if (!frameNode->attached()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    ASSERT(frameNode->isElementNode());
    if (!frameNode->isElementNode())
        return false;

    Element* element = static_cast<Element*>(frameNode);
    ASSERT(element->isFrameOwnerElement());
    if (!element->isFrameOwnerElement())
        return false;

    HTMLFrameOwnerElement* frameOwner = static_cast<HTMLFrameOwnerElement*>(element);
    ASSERT(frameOwner->contentFrame());
    if (!frameOwner->contentFrame())
        return false;

    FrameLoader* loader = frameOwner->contentFrame()->loader();

    loader->setResponseMIMEType(mimeType);
    loader->begin();
    loader->write(source);
    loader->end();

    return true;
}

String InspectorFrontendHost::setting(const String& key)
{
    return m_inspectorController ? m_inspectorController->setting(key) : "";
}

void InspectorFrontendHost::setSetting(const String& key, const String& value)
{
    if (m_inspectorController)
        m_inspectorController->setSetting(key, value);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
