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

#ifndef WebKit2GtkAuthenticationDialog_h
#define WebKit2GtkAuthenticationDialog_h

#include "AuthenticationChallengeProxy.h"
#include "WKRetainPtr.h"
#include "WebKitWebViewBase.h"
#include <WebCore/Credential.h>
#include <WebCore/GtkAuthenticationDialog.h>
#include <wtf/gobject/GRefPtr.h>

namespace WebKit {

class WebKit2GtkAuthenticationDialog : public WebCore::GtkAuthenticationDialog {
public:
    WebKit2GtkAuthenticationDialog(AuthenticationChallengeProxy*, CredentialStorageMode);
    virtual ~WebKit2GtkAuthenticationDialog() { }
    GtkWidget* widget() { return m_dialog; }

protected:
    virtual void authenticate(const WebCore::Credential&);

private:
    RefPtr<AuthenticationChallengeProxy> m_authenticationChallenge;
    GRefPtr<GtkStyleContext> m_styleContext;
};

} // namespace WebKit

#endif // WebKit2GtkAuthenticationDialog_h
