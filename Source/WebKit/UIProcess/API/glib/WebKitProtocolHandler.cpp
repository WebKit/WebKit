/*
 * Copyright (C) 2019 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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
#include "WebKitProtocolHandler.h"

#include "BuildRevision.h"
#include "WebKitError.h"
#include "WebKitVersion.h"
#include "WebKitWebView.h"
#include <WebCore/FloatRect.h>
#include <WebCore/GLContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/PlatformScreen.h>
#include <gio/gio.h>
#include <wtf/URL.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

#if OS(UNIX)
#include <sys/utsname.h>
#endif

#if USE(CAIRO)
#include <cairo.h>
#endif

#if PLATFORM(GTK)
#include <gtk/gtk.h>

#if PLATFORM(WAYLAND)
#include <wpe/wpe.h>
#include <wpe/fdo.h>
#endif
#endif

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#elif USE(OPENGL_ES)
#include <GLES2/gl2.h>
#else
#include <WebCore/OpenGLShims.h>
#endif

#if USE(EGL)
#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#endif
#endif

#if PLATFORM(X11)
#include <WebCore/PlatformDisplayX11.h>
#if USE(GLX)
#if USE(LIBEPOXY)
#include <epoxy/glx.h>
#else
#include <GL/glx.h>
#endif
#endif
#endif

#if USE(GSTREAMER)
#include <gst/gst.h>
#endif

namespace WebKit {
using namespace WebCore;

WebKitProtocolHandler::WebKitProtocolHandler(WebKitWebContext* context)
{
    webkit_web_context_register_uri_scheme(context, "webkit", [](WebKitURISchemeRequest* request, gpointer userData) {
        static_cast<WebKitProtocolHandler*>(userData)->handleRequest(request);
    }, this, nullptr);

    auto* manager = webkit_web_context_get_security_manager(context);
    webkit_security_manager_register_uri_scheme_as_display_isolated(manager, "webkit");
    webkit_security_manager_register_uri_scheme_as_local(manager, "webkit");
}

void WebKitProtocolHandler::handleRequest(WebKitURISchemeRequest* request)
{
    URL requestURL = URL(String::fromLatin1(webkit_uri_scheme_request_get_uri(request)));
    if (requestURL.host() == "gpu"_s) {
        handleGPU(request);
        return;
    }

    GUniquePtr<GError> error(g_error_new_literal(WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI, "Not found"));
        webkit_uri_scheme_request_finish_error(request, error.get());
}

static inline const char* webkitPortName()
{
#if PLATFORM(GTK)
    return "WebKitGTK";
#elif PLATFORM(WPE)
    return "WPE WebKit";
#endif
    RELEASE_ASSERT_NOT_REACHED();
}

static const char* hardwareAccelerationPolicy(WebKitURISchemeRequest* request)
{
#if PLATFORM(WPE)
    return "always";
#elif PLATFORM(GTK)
    auto* webView = webkit_uri_scheme_request_get_web_view(request);
    ASSERT(webView);

    switch (webkit_settings_get_hardware_acceleration_policy(webkit_web_view_get_settings(webView))) {
    case WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER:
        return "never";
    case WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS:
        return "always";
    case WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND:
        return "on demand";
    }
#endif
    RELEASE_ASSERT_NOT_REACHED();
}

static bool webGLEnabled(WebKitURISchemeRequest* request)
{
    auto* webView = webkit_uri_scheme_request_get_web_view(request);
    ASSERT(webView);
    return webkit_settings_get_enable_webgl(webkit_web_view_get_settings(webView));
}

static const char* openGLAPI(bool isEGL)
{
#if USE(LIBEPOXY)
    if (epoxy_is_desktop_gl())
        return "OpenGL (libepoxy)";
    return "OpenGL ES 2 (libepoxy)";
#else
#if USE(EGL)
    if (isEGL) {
#if USE(OPENGL_ES)
        return "OpenGL ES 2";
#endif
    }
#endif
    return "OpenGL";
#endif
    RELEASE_ASSERT_NOT_REACHED();
}

void WebKitProtocolHandler::handleGPU(WebKitURISchemeRequest* request)
{
    GString* html = g_string_new(
        "<html><head><title>GPU information</title>"
        "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
        "<style>"
        "  h1 { color: #babdb6; text-shadow: 0 1px 0 white; margin-bottom: 0; }"
        "  html { font-family: -webkit-system-font; font-size: 11pt; color: #2e3436; padding: 20px 20px 0 20px; background-color: #f6f6f4; "
        "         background-image: -webkit-gradient(linear, left top, left bottom, color-stop(0, #eeeeec), color-stop(1, #f6f6f4));"
        "         background-size: 100% 5em; background-repeat: no-repeat; }"
        "  table { width: 100%; border-collapse: collapse; }"
        "  table, td { border: 1px solid #d3d7cf; border-left: none; border-right: none; }"
        "  p { margin-bottom: 30px; }"
        "  td { padding: 15px; }"
        "  td.data { width: 200px; }"
        "  .titlename { font-weight: bold; }"
        "</style></head><body>");

    g_string_append(html,
        "<h1>Version Information</h1>"
        "<table>");

    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">WebKit version</div></td>"
        "  <td>%s %d.%d.%d (%s)</td>"
        " </tbody></tr>",
        webkitPortName(), WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION, WEBKIT_MICRO_VERSION, BUILD_REVISION);

#if OS(UNIX)
    struct utsname osName;
    uname(&osName);
    g_string_append_printf(
        html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">Operating system</div></td>"
        "  <td>%s %s %s %s</td>"
        " </tbody></tr>",
        osName.sysname, osName.release, osName.version, osName.machine);
#endif

    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">Desktop</div></td>"
        "  <td>%s</td>"
        " </tbody></tr>",
        g_getenv("XDG_CURRENT_DESKTOP"));

#if USE(CAIRO)
    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">Cairo version</div></td>"
        "  <td>%s (build) %s (runtime)</td>"
        " </tbody></tr>",
        CAIRO_VERSION_STRING, cairo_version_string());
#endif

#if USE(GSTREAMER)
    GUniquePtr<char> gstVersion(gst_version_string());
    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">GStreamer version</div></td>"
        "  <td>%d.%d.%d (build) %s (runtime)</td>"
        " </tbody></tr>",
        GST_VERSION_MAJOR, GST_VERSION_MINOR, GST_VERSION_MICRO, gstVersion.get());
#endif

#if PLATFORM(GTK)
    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">GTK version</div></td>"
        "  <td>%d.%d.%d (build) %d.%d.%d (runtime)</td>"
        " </tbody></tr>",
        GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
        gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version());

#if PLATFORM(WAYLAND)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland) {
        g_string_append_printf(html,
            " <tbody><tr>"
            "  <td><div class=\"titlename\">WPE version</div></td>"
            "  <td>%d.%d.%d (build) %d.%d.%d (runtime)</td>"
            " </tbody></tr>",
            WPE_MAJOR_VERSION, WPE_MINOR_VERSION, WPE_MICRO_VERSION,
            wpe_get_major_version(), wpe_get_minor_version(), wpe_get_micro_version());

        g_string_append_printf(html,
            " <tbody><tr>"
            "  <td><div class=\"titlename\">WPEBackend-fdo version</div></td>"
            "  <td>%d.%d.%d (build) %d.%d.%d (runtime)</td>"
            " </tbody></tr>",
            WPE_FDO_MAJOR_VERSION, WPE_FDO_MINOR_VERSION, WPE_FDO_MICRO_VERSION,
            wpe_fdo_get_major_version(), wpe_fdo_get_minor_version(), wpe_fdo_get_micro_version());
    }
#endif
#endif

#if PLATFORM(WPE)
    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">WPE version</div></td>"
        "  <td>%d.%d.%d (build) %d.%d.%d (runtime)</td>"
        " </tbody></tr>",
        WPE_MAJOR_VERSION, WPE_MINOR_VERSION, WPE_MICRO_VERSION,
        wpe_get_major_version(), wpe_get_minor_version(), wpe_get_micro_version());

    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">WPE backend</div></td>"
        "  <td>%s</td>"
        " </tbody></tr>",
        wpe_loader_get_loaded_implementation_library_name());
#endif
    g_string_append(html, "<table>");

    g_string_append(html,
        "<h1>Display Information</h1>"
        "<table>");

#if PLATFORM(GTK)
    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">Type</div></td>"
        "  <td>%s</td>"
        " </tbody></tr>",
#if PLATFORM(WAYLAND)
        PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland ? "Wayland" :
#endif
#if PLATFORM(X11)
        PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11 ? "X11" :
#endif
        "Unknown"
    );
#endif

    auto rect = IntRect(screenRect(nullptr));
    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">Screen geometry</div></td>"
        "  <td>%d,%d %dx%d</td>"
        " </tbody></tr>",
        rect.x(), rect.y(), rect.width(), rect.height());

    rect = IntRect(screenAvailableRect(nullptr));
    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">Screen work area</div></td>"
        "  <td>%d,%d %dx%d</td>"
        " </tbody></tr>",
        rect.x(), rect.y(), rect.width(), rect.height());

    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">Depth</div></td>"
        "  <td>%d</td>"
        " </tbody></tr>",
        screenDepth(nullptr));

    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">Bits per color component</div></td>"
        "  <td>%d</td>"
        " </tbody></tr>",
        screenDepthPerComponent(nullptr));

    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">DPI</div></td>"
        "  <td>%.2f</td>"
        " </tbody></tr>",
        screenDPI());

    g_string_append(html, "<table>");

    g_string_append(html,
        "<h1>Hardware Acceleration Information</h1>"
        "<table>");

    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">Policy</div></td>"
        "  <td>%s</td>"
        " </tbody></tr>",
        hardwareAccelerationPolicy(request));

#if ENABLE(WEBGL)
    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">WebGL enabled</div></td>"
        "  <td>%s</td>"
        " </tbody></tr>",
        webGLEnabled(request) ? "Yes" : "No");
#endif

#if USE(EGL) || USE(GLX)
    auto glContext = GLContext::createOffscreenContext();
    glContext->makeContextCurrent();

    bool isEGL = glContext->isEGLContext();

    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">API</div></td>"
        "  <td>%s</td>"
        " </tbody></tr>",
        openGLAPI(isEGL));

    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">Native interface</div></td>"
        "  <td>%s</td>"
        " </tbody></tr>",
        isEGL ? "EGL" : "GLX");

    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">GL_RENDERER</div></td>"
        "  <td>%s</td>"
        " </tbody></tr>",
        reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">GL_VENDOR</div></td>"
        "  <td>%s</td>"
        " </tbody></tr>",
        reinterpret_cast<const char*>(glGetString(GL_VENDOR)));

    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">GL_VERSION</div></td>"
        "  <td>%s</td>"
        " </tbody></tr>",
        reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">GL_SHADING_LANGUAGE_VERSION</div></td>"
        "  <td>%s</td>"
        " </tbody></tr>",
        reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

#if USE(OPENGL_ES)
    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">GL_EXTENSIONS</div></td>"
        "  <td>%s</td>"
        " </tbody></tr>",
        reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
#else
    GString* extensions = g_string_new(nullptr);
    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    for (GLint i = 0; i < numExtensions; ++i) {
        if (i)
            g_string_append_c(extensions, ' ');
        g_string_append(extensions, reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i)));
    }
    g_string_append_printf(html,
        " <tbody><tr>"
        "  <td><div class=\"titlename\">GL_EXTENSIONS</div></td>"
        "  <td>%s</td>"
        " </tbody></tr>",
        extensions->str);
    g_string_free(extensions, TRUE);
#endif

#if USE(GLX)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11 && !isEGL) {
        auto* x11Display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();

        g_string_append_printf(html,
            " <tbody><tr>"
            "  <td><div class=\"titlename\">GLX_VERSION</div></td>"
            "  <td>%s</td>"
            " </tbody></tr>",
            glXGetClientString(x11Display, GLX_VERSION));

        g_string_append_printf(html,
            " <tbody><tr>"
            "  <td><div class=\"titlename\">GLX_VENDOR</div></td>"
            "  <td>%s</td>"
            " </tbody></tr>",
            glXGetClientString(x11Display, GLX_VENDOR));

        g_string_append_printf(html,
            " <tbody><tr>"
            "  <td><div class=\"titlename\">GLX_EXTENSIONS</div></td>"
            "  <td>%s</td>"
            " </tbody></tr>",
            glXGetClientString(x11Display, GLX_EXTENSIONS));
    }
#endif

#if USE(EGL)
    if (isEGL) {
        auto eglDisplay = PlatformDisplay::sharedDisplay().eglDisplay();

        g_string_append_printf(html,
            " <tbody><tr>"
            "  <td><div class=\"titlename\">EGL_VERSION</div></td>"
            "  <td>%s</td>"
            " </tbody></tr>",
            eglQueryString(eglDisplay, EGL_VERSION));

        g_string_append_printf(html,
            " <tbody><tr>"
            "  <td><div class=\"titlename\">EGL_VENDOR</div></td>"
            "  <td>%s</td>"
            " </tbody></tr>",
            eglQueryString(eglDisplay, EGL_VENDOR));

        g_string_append_printf(html,
            " <tbody><tr>"
            "  <td><div class=\"titlename\">EGL_EXTENSIONS</div></td>"
            "  <td>%s %s</td>"
            " </tbody></tr>",
            eglQueryString(nullptr, EGL_EXTENSIONS),
            eglQueryString(eglDisplay, EGL_EXTENSIONS));
    }
#endif
#endif // USE(EGL) || USE(GLX)

    g_string_append(html, "<table>");

    g_string_append(html, "</body></html>");
    gsize streamLength = html->len;
    GRefPtr<GInputStream> stream = adoptGRef(g_memory_input_stream_new_from_data(g_string_free(html, FALSE), streamLength, g_free));
    webkit_uri_scheme_request_finish(request, stream.get(), streamLength, "text/html");
}

} // namespace WebKit
