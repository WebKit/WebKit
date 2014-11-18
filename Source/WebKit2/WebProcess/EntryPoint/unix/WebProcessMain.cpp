/*
 * Copyright (C) 2014 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebProcessMainUnix.h"

#include <cstdlib>

using namespace WebKit;

int main(int argc, char** argv)
{
    // Disable SSLv3 very early because it is practically impossible to safely
    // use setenv() when multiple threads are running, as another thread calling
    // getenv() could cause a crash, and many functions use getenv() internally.
    // This workaround will stop working if glib-networking switches away from
    // GnuTLS or simply stops parsing this variable. We intentionally do not
    // overwrite this priority string if it's already set by the user.
    // Keep this in sync with NetworkProcessMain.cpp.
    // https://bugzilla.gnome.org/show_bug.cgi?id=738633
    setenv("G_TLS_GNUTLS_PRIORITY", "NORMAL:%COMPAT:%LATEST_RECORD_VERSION:!VERS-SSL3.0", 0);

    return WebProcessMainUnix(argc, argv);
}
