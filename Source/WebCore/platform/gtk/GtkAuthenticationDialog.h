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

#define LIBSOUP_I_HAVE_READ_BUG_594377_AND_KNOW_SOUP_PASSWORD_MANAGER_MIGHT_GO_AWAY

#include <wtf/gobject/GOwnPtr.h>
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
    GtkAuthenticationDialog(GtkWindow*, SoupSession*, SoupMessage*, SoupAuth*);
    ~GtkAuthenticationDialog();

    void show();

private:
    void destroy();
    void authenticate();

#ifdef SOUP_TYPE_PASSWORD_MANAGER
    void savePassword();
    static void savePasswordCallback(SoupMessage*, GtkAuthenticationDialog*);
#endif

    static void authenticationDialogResponseCallback(GtkWidget*, gint responseID, GtkAuthenticationDialog*);

    GtkWidget* m_dialog;
    SoupSession* m_session;
    GRefPtr<SoupMessage> m_message;
    SoupAuth* m_auth;

    GtkWidget* m_loginEntry;
    GtkWidget* m_passwordEntry;
    GtkWidget* m_rememberCheckButton;

#ifdef SOUP_TYPE_PASSWORD_MANAGER
    bool m_isSavingPassword;
    unsigned long m_savePasswordHandler;
    CString m_username;
    CString m_password;
#endif
};

} // namespace WebCore

#endif // GtkAuthenticationDialog_h
