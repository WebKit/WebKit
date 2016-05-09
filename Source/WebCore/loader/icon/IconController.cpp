/*
 * Copyright (C) 2006-2011, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 * Copyright (C) 2011 Kris Jordan <krisjordan@gmail.com>
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "IconController.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "ElementChildIterator.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLHeadElement.h"
#include "HTMLLinkElement.h"
#include "IconDatabase.h"
#include "IconLoader.h"
#include "LinkIconType.h"
#include "Logging.h"
#include "MainFrame.h"
#include "Page.h"
#include "Settings.h"
#include <wtf/text/CString.h>

namespace WebCore {

IconController::IconController(Frame& frame)
    : m_frame(frame)
{
}

IconController::~IconController()
{
}

static URL iconFromLinkElements(Frame& frame)
{
    // This function returns the first icon with a mime type.
    // If no icon with mime type exists, the last icon is returned.
    // It may make more sense to always return the last icon,
    // but this implementation is consistent with previous behavior.

    URL result;

    auto* document = frame.document();
    if (!document)
        return result;

    auto* head = document->head();
    if (!head)
        return result;

    for (auto& linkElement : childrenOfType<HTMLLinkElement>(*head)) {
        if (!linkElement.iconType())
            continue;
        if (*linkElement.iconType() != LinkIconType::Favicon)
            continue;
        if (linkElement.href().isEmpty())
            continue;
        result = linkElement.href();
        if (!linkElement.type().isEmpty())
            break;
    }

    return result;
}

URL IconController::url()
{
    if (!m_frame.isMainFrame())
        return URL();

    auto icon = iconFromLinkElements(m_frame);
    if (!icon.isEmpty())
        return icon;

    icon = m_frame.document()->completeURL(ASCIILiteral("/favicon.ico"));
    if (icon.protocolIsInHTTPFamily()) {
        // FIXME: Not sure we need to remove credentials like this.
        // However this preserves behavior this code path has historically had.
        icon.setUser(String());
        icon.setPass(String());
        return icon;
    }

    return URL();
}

void IconController::commitToDatabase(const URL& icon)
{
    LOG(IconDatabase, "Committing iconURL %s to database for pageURLs %s and %s", icon.string().ascii().data(), m_frame.document()->url().string().ascii().data(), m_frame.loader().initialRequest().url().string().ascii().data());
    iconDatabase().setIconURLForPageURL(icon.string(), m_frame.document()->url().string());
    iconDatabase().setIconURLForPageURL(icon.string(), m_frame.loader().initialRequest().url().string());
}

void IconController::startLoader()
{
    // FIXME: We kick off the icon loader when the frame is done receiving its main resource.
    // But we should instead do it when we're done parsing the head element.

    if (!m_frame.isMainFrame())
        return;

    if (!iconDatabase().isEnabled())
        return;

    ASSERT(!m_frame.tree().parent());
    if (!documentCanHaveIcon(m_frame.document()->url()))
        return;

    URL iconURL = url();
    if (iconURL.isEmpty())
        return;

    // People who want to avoid loading images generally want to avoid loading all images, unless an exception has been made for site icons.
    // Now that we've accounted for URL mapping, avoid starting the network load if images aren't set to display automatically.
    if (!m_frame.settings().loadsImagesAutomatically() && !m_frame.settings().loadsSiteIconsIgnoringImageLoadingSetting())
        return;

    // If we're reloading the page, always start the icon load now.
    // FIXME: How can this condition ever be true?
    if (m_frame.loader().loadType() == FrameLoadType::Reload && m_frame.loader().loadType() == FrameLoadType::ReloadFromOrigin) {
        continueLoadWithDecision(IconLoadYes);
        return;
    }

    if (iconDatabase().supportsAsynchronousMode()) {
        // FIXME (<rdar://problem/9168605>) - We should support in-memory-only private browsing icons in asynchronous icon database mode.
        if (m_frame.page() && m_frame.page()->usesEphemeralSession())
            return;

        m_frame.loader().documentLoader()->getIconLoadDecisionForIconURL(iconURL.string());
        // Commit the icon url mapping to the database just in case we don't end up loading later.
        commitToDatabase(iconURL);
        return;
    }

    IconLoadDecision decision = iconDatabase().synchronousLoadDecisionForIconURL(iconURL.string(), m_frame.loader().documentLoader());

    if (decision == IconLoadUnknown) {
        // In this case, we may end up loading the icon later, but we still want to commit the icon url mapping to the database
        // just in case we don't end up loading later - if we commit the mapping a second time after the load, that's no big deal
        // We also tell the client to register for the notification that the icon is received now so it isn't missed in case the 
        // icon is later read in from disk
        LOG(IconDatabase, "IconController %p might load icon %s later", this, iconURL.string().utf8().data());
        m_waitingForLoadDecision = true;    
        m_frame.loader().client().registerForIconNotification();
        commitToDatabase(iconURL);
        return;
    }

    continueLoadWithDecision(decision);
}

void IconController::stopLoader()
{
    if (m_iconLoader)
        m_iconLoader->stopLoading();
}

// Callback for the old-style synchronous IconDatabase interface.
void IconController::loadDecisionReceived(IconLoadDecision iconLoadDecision)
{
    if (!m_waitingForLoadDecision)
        return;
    LOG(IconDatabase, "IconController %p was told a load decision is available for its icon", this);
    continueLoadWithDecision(iconLoadDecision);
    m_waitingForLoadDecision = false;
}

void IconController::continueLoadWithDecision(IconLoadDecision iconLoadDecision)
{
    ASSERT(iconLoadDecision != IconLoadUnknown);

    if (iconLoadDecision == IconLoadNo) {
        URL iconURL = url();
        if (iconURL.isEmpty())
            return;

        LOG(IconDatabase, "IconController::startLoader() - Told not to load this icon, committing iconURL %s to database for pageURL mapping", iconURL.string().utf8().data());
        commitToDatabase(iconURL);

        if (iconDatabase().supportsAsynchronousMode()) {
            m_frame.loader().documentLoader()->getIconDataForIconURL(iconURL.string());
            return;
        }

        // We were told not to load this icon - that means this icon is already known by the database
        // If the icon data hasn't been read in from disk yet, kick off the read of the icon from the database to make sure someone
        // has done it. This is after registering for the notification so the WebView can call the appropriate delegate method.
        // Otherwise if the icon data *is* available, notify the delegate
        if (!iconDatabase().synchronousIconDataKnownForIconURL(iconURL.string())) {
            LOG(IconDatabase, "Told not to load icon %s but icon data is not yet available - registering for notification and requesting load from disk", iconURL.string().ascii().data());
            m_frame.loader().client().registerForIconNotification();
            iconDatabase().synchronousIconForPageURL(m_frame.document()->url().string(), IntSize(0, 0));
            iconDatabase().synchronousIconForPageURL(m_frame.loader().initialRequest().url().string(), IntSize(0, 0));
        } else
            m_frame.loader().client().dispatchDidReceiveIcon();

        return;
    } 

    if (!m_iconLoader)
        m_iconLoader = std::make_unique<IconLoader>(m_frame);

    m_iconLoader->startLoading();
}

}
