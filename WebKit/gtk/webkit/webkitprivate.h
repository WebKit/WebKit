/*
 * Copyright (C) 2007, 2008, 2009 Holger Hans Peter Freyther
 * Copyright (C) 2008 Jan Michael C. Alonzo
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2010 Igalia S.L.
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
 */

#ifndef webkitprivate_h
#define webkitprivate_h

/*
 * This file knows the shared secret of WebKitWebView, WebKitWebFrame,
 * and WebKitNetworkRequest.
 * They are using WebCore which musn't be exposed to the outer world.
 */

#include <webkit/webkitdefines.h>
#include <webkit/webkitnetworkrequest.h>
#include <webkit/webkitwebview.h>
#include <webkit/webkitwebdatasource.h>
#include <webkit/webkitwebframe.h>
#include <webkit/webkitwebsettings.h>
#include <webkit/webkitwebwindowfeatures.h>
#include <webkit/webkitnetworkrequest.h>
#include <webkit/webkitsecurityorigin.h>

#include "DataObjectGtk.h"
#include "DragActions.h"
#include "Frame.h"
#include "GOwnPtr.h"
#include "Geolocation.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "FrameLoaderClient.h"
#include "Node.h"
#include "Page.h"
#include "PlatformString.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "WindowFeatures.h"
#include "Settings.h"
#include <enchant.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/CString.h>

#include <atk/atk.h>
#include <glib.h>
#include <libsoup/soup.h>

namespace WebKit {

    class DocumentLoader;
    class PasteboardHelperGtk;

    WebCore::EditingBehaviorType core(WebKitEditingBehavior type);

    PasteboardHelperGtk* pasteboardHelperInstance();
}

extern "C" {
    void webkit_init();

#define WEBKIT_PARAM_READABLE ((GParamFlags)(G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB))
#define WEBKIT_PARAM_READWRITE ((GParamFlags)(G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB))

    WTF::String
    webkitUserAgent();

    WebKitWebWindowFeatures*
    webkit_web_window_features_new_from_core_features (const WebCore::WindowFeatures& features);

    WebKitGeolocationPolicyDecision*
    webkit_geolocation_policy_decision_new(WebKitWebFrame*, WebCore::Geolocation*);

    WEBKIT_API void
    webkit_application_cache_set_maximum_size(unsigned long long size);

    // WebKitWebDataSource private
    WebKitWebDataSource*
    webkit_web_data_source_new_with_loader(PassRefPtr<WebKit::DocumentLoader>);
}

#endif
