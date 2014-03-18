/*
 *  Copyright (C) 2014 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ProgressTrackerClientGtk.h"

#include "Page.h"
#include "ProgressTracker.h"
#include "webkitwebframeprivate.h"
#include "webkitwebviewprivate.h"

namespace WebKit {

ProgressTrackerClient::ProgressTrackerClient(WebKitWebView* webView)
    : m_webView(webView)
{
    ASSERT(m_webView);
}

void ProgressTrackerClient::progressTrackerDestroyed()
{
    delete this;
}

void ProgressTrackerClient::progressStarted(WebCore::Frame& originatingProgressFrame)
{
    WebKitWebFrame* frame = kit(&originatingProgressFrame);
    ASSERT(m_webView == getViewFromFrame(frame));

    g_signal_emit_by_name(m_webView, "load-started", frame);

    g_object_notify(G_OBJECT(m_webView), "progress");
}

void ProgressTrackerClient::progressEstimateChanged(WebCore::Frame& originatingProgressFrame)
{
    ASSERT_UNUSED(originatingProgressFrame, (m_webView == getViewFromFrame(kit(&originatingProgressFrame))));

    WebCore::Page* corePage = core(m_webView);
    g_signal_emit_by_name(m_webView, "load-progress-changed", lround(corePage->progress().estimatedProgress()*100));

    g_object_notify(G_OBJECT(m_webView), "progress");
}

void ProgressTrackerClient::progressFinished(WebCore::Frame& originatingProgressFrame)
{
    WebKitWebFrame* frame = kit(&originatingProgressFrame);
    ASSERT(m_webView == getViewFromFrame(frame));

    // We can get a stopLoad() from dispose when the object is being
    // destroyed, don't emit the signal in that case.
    if (!m_webView->priv->disposing)
        g_signal_emit_by_name(m_webView, "load-finished", frame);
}

} // namespace WebKit
