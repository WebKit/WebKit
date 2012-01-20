/*
 * Copyright (C) 2011 Igalia S.L.
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

#include "config.h"
#include "PluginProcessMainGtk.h"

#include "PluginProcess.h"
#include <WebCore/RunLoop.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <runtime/InitializeThreading.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

static int webkitgtkXError(Display* xdisplay, XErrorEvent* error)
{
    gchar errorMessage[64];
    XGetErrorText(xdisplay, error->error_code, errorMessage, 63);
    g_warning("The program '%s' received an X Window System error.\n"
              "This probably reflects a bug in a browser plugin.\n"
              "The error was '%s'.\n"
              "  (Details: serial %ld error_code %d request_code %d minor_code %d)\n",
              g_get_prgname(), errorMessage,
              error->serial, error->error_code,
              error->request_code, error->minor_code);
    return 0;
}

WK_EXPORT int PluginProcessMainGtk(int argc, char* argv[])
{
    ASSERT(argc == 2);

    gtk_init(&argc, &argv);

    JSC::initializeThreading();
    WTF::initializeMainThread();
    RunLoop::initializeMainRunLoop();

    // Plugins can produce X errors that are handled by the GDK X error handler, which
    // exits the process. Since we don't want to crash due to plugin bugs, we install a
    // custom error handler to show a warning when a X error happens without aborting.
    XSetErrorHandler(webkitgtkXError);

    int socket = atoi(argv[1]);
    WebKit::PluginProcess::shared().initialize(socket, RunLoop::main());
    RunLoop::run();

    return 0;
}

} // namespace WebKit


