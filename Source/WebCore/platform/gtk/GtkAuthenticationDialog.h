/*
 * Copyright (C) 2009, 2011 Igalia S.L.
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

#ifndef GtkAuthenticationDialog_h
#define GtkAuthenticationDialog_h

#include "AuthenticationChallenge.h"
#include "GRefPtrGtk.h"
#include <libsoup/soup.h>
#include <wtf/FastAllocBase.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/CString.h>

namespace WebCore {

class GtkAuthenticationDialog {
    WTF_MAKE_NONCOPYABLE(GtkAuthenticationDialog);
    WTF_MAKE_FAST_ALLOCATED;

public:
    enum CredentialStorageMode {
        AllowPersistentStorage, // The user is asked whether to store credential information.
        DisallowPersistentStorage // Credential information is only kept in the session.
    };

    GtkAuthenticationDialog(const AuthenticationChallenge&, CredentialStorageMode);
    GtkAuthenticationDialog(GtkWindow*, const AuthenticationChallenge&, CredentialStorageMode);
    virtual ~GtkAuthenticationDialog() { }
    void show();
    void destroy();

protected:
    void createContentsInContainer(GtkWidget* container);
    virtual void authenticate(const Credential&);
    GtkWidget* m_dialog;
    GtkWidget* m_loginEntry;
    GtkWidget* m_passwordEntry;
    GtkWidget* m_rememberCheckButton;
    GtkWidget* m_okayButton;
    GtkWidget* m_cancelButton;

private:
    static void buttonClickedCallback(GtkWidget*, GtkAuthenticationDialog*);
    AuthenticationChallenge m_challenge;
    CredentialStorageMode m_credentialStorageMode;
};

} // namespace WebCore

#endif // GtkAuthenticationDialog_h
