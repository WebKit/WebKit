/*
 * Copyright (C) 2012 Igalia S.L.
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

#include "config.h"
#include "WebKit2GtkAuthenticationDialog.h"

#include "AuthenticationChallengeProxy.h"
#include "AuthenticationDecisionListener.h"
#include "WebCredential.h"

namespace WebKit {

WebKit2GtkAuthenticationDialog::WebKit2GtkAuthenticationDialog(AuthenticationChallengeProxy* authenticationChallenge)
    : GtkAuthenticationDialog(0, authenticationChallenge->core())
    , m_authenticationChallenge(authenticationChallenge)
{
    // We aren't passing a toplevel to the GtkAuthenticationDialog constructor,
    // because eventually this widget will be embedded into the WebView itself.
}

void WebKit2GtkAuthenticationDialog::authenticate(const WebCore::Credential& credential)
{
    RefPtr<WebCredential> webCredential = WebCredential::create(credential);
    m_authenticationChallenge->listener()->useCredential(webCredential.get());
}

} // namespace WebKit
