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
#if !USE(GTK4)
    case WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND:
        return "on demand";
#endif
    }
#endif
    RELEASE_ASSERT_NOT_REACHED();
}

#if ENABLE(WEBGL)
static bool webGLEnabled(WebKitURISchemeRequest* request)
{
    auto* webView = webkit_uri_scheme_request_get_web_view(request);
    ASSERT(webView);
    return webkit_settings_get_enable_webgl(webkit_web_view_get_settings(webView));
}
#endif

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
        "</style>");

    StringBuilder tablesBuilder;

    auto startTable = [&](auto header) {
        tablesBuilder.append("<h1>"_s, header, "</h1><table>"_s);
    };

    auto addTableRow = [&](auto& jsonObject, auto key, auto&& value) {
        tablesBuilder.append("<tbody><tr><td><div class=\"titlename\">"_s, key, "</div></td><td>"_s, value, "</td></tr></tbody>"_s);
        jsonObject->setString(key, value);
    };

    auto stopTable = [&] {
        tablesBuilder.append("</table>"_s);
    };

    auto jsonObject = JSON::Object::create();

    startTable("Version Information"_s);
    auto versionObject = JSON::Object::create();
    addTableRow(versionObject, "WebKit version"_s, makeString(webkitPortName(), ' ', WEBKIT_MAJOR_VERSION, '.', WEBKIT_MINOR_VERSION, '.', WEBKIT_MICRO_VERSION, " ("_s, BUILD_REVISION, ')'));

#if OS(UNIX)
    struct utsname osName;
    uname(&osName);
    addTableRow(versionObject, "Operating system"_s, makeString(osName.sysname, ' ', osName.release, ' ', osName.version, ' ', osName.machine));
#endif

    addTableRow(versionObject, "Desktop"_s, makeString(g_getenv("XDG_CURRENT_DESKTOP")));

#if USE(CAIRO)
    addTableRow(versionObject, "Cairo version"_s, makeString(CAIRO_VERSION_STRING, " (build) "_s, cairo_version_string(), " (runtime)"_s));
#endif

#if USE(GSTREAMER)
    GUniquePtr<char> gstVersion(gst_version_string());
    addTableRow(versionObject, "GStreamer version"_s, makeString(GST_VERSION_MAJOR, '.', GST_VERSION_MINOR, '.', GST_VERSION_MICRO, " (build) "_s, gstVersion.get(), " (runtime)"_s));
#endif

#if PLATFORM(GTK)
    addTableRow(versionObject, "GTK version"_s, makeString(GTK_MAJOR_VERSION, '.', GTK_MINOR_VERSION, '.', GTK_MICRO_VERSION, " (build) "_s, gtk_get_major_version(), '.', gtk_get_minor_version(), '.', gtk_get_micro_version(), " (runtime)"_s));

#if PLATFORM(WAYLAND)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland) {
        addTableRow(versionObject, "WPE version"_s, makeString(WPE_MAJOR_VERSION, '.', WPE_MINOR_VERSION, '.', WPE_MICRO_VERSION, " (build) "_s, wpe_get_major_version(), '.', wpe_get_minor_version(), '.', wpe_get_micro_version(), " (runtime)"_s));

#if WPE_FDO_CHECK_VERSION(1, 6, 1)
        addTableRow(versionObject, "WPEBackend-fdo version"_s, makeString(WPE_FDO_MAJOR_VERSION, '.', WPE_FDO_MINOR_VERSION, '.', WPE_FDO_MICRO_VERSION, " (build) "_s, wpe_fdo_get_major_version(), '.', wpe_fdo_get_minor_version(), '.', wpe_fdo_get_micro_version(), " (runtime)"_s));
#endif
    }
#endif
#endif

#if PLATFORM(WPE)
    addTableRow(versionObject, "WPE version"_s, makeString(WPE_MAJOR_VERSION, '.', WPE_MINOR_VERSION, '.', WPE_MICRO_VERSION, " (build) "_s, wpe_get_major_version(), '.', wpe_get_minor_version(), '.', wpe_get_micro_version(), " (runtime)"_s));
    addTableRow(versionObject, "WPE backend"_s, makeString(wpe_loader_get_loaded_implementation_library_name()));
#endif

    stopTable();
    jsonObject->setObject("Version Information"_s, WTFMove(versionObject));

    auto displayObject = JSON::Object::create();
    startTable("Display Information"_s);

#if PLATFORM(GTK)
    auto typeString =
#if PLATFORM(WAYLAND)
        PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland ? "Wayland"_s :
#endif
#if PLATFORM(X11)
        PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11 ? "X11"_s :
#endif
        "Unknown"_s;
    addTableRow(displayObject, "Type"_s, WTFMove(typeString));
#endif // PLATFORM(GTK)

    auto rect = IntRect(screenRect(nullptr));
    addTableRow(displayObject, "Screen geometry"_s, makeString(rect.x(), ',', rect.y(), ' ', rect.width(), 'x', rect.height()));

    rect = IntRect(screenAvailableRect(nullptr));
    addTableRow(displayObject, "Screen work area"_s, makeString(rect.x(), ',', rect.y(), ' ', rect.width(), 'x', rect.height()));
    addTableRow(displayObject, "Depth"_s, makeString(screenDepth(nullptr)));
    addTableRow(displayObject, "Bits per color component"_s, makeString(screenDepthPerComponent(nullptr)));
    addTableRow(displayObject, "DPI"_s, makeString(screenDPI()));

    stopTable();
    jsonObject->setObject("Display Information"_s, WTFMove(displayObject));

    auto hardwareAccelerationObject = JSON::Object::create();
    startTable("Hardware Acceleration Information"_s);
    addTableRow(hardwareAccelerationObject, "Policy"_s, makeString(hardwareAccelerationPolicy(request)));

#if ENABLE(WEBGL)
    addTableRow(hardwareAccelerationObject, "WebGL enabled"_s, webGLEnabled(request) ? "Yes"_s : "No"_s);
#endif

#if USE(EGL) || USE(GLX)
    auto glContext = GLContext::createOffscreenContext();
    glContext->makeContextCurrent();

    bool isEGL = glContext->isEGLContext();

    addTableRow(hardwareAccelerationObject, "API"_s, makeString(openGLAPI(isEGL)));
    addTableRow(hardwareAccelerationObject, "Native interface"_s, isEGL ? "EGL"_s : "GLX"_s);
    addTableRow(hardwareAccelerationObject, "GL_RENDERER"_s, makeString(reinterpret_cast<const char*>(glGetString(GL_RENDERER))));
    addTableRow(hardwareAccelerationObject, "GL_VENDOR"_s, makeString(reinterpret_cast<const char*>(glGetString(GL_VENDOR))));
    addTableRow(hardwareAccelerationObject, "GL_VERSION"_s, makeString(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
    addTableRow(hardwareAccelerationObject, "GL_SHADING_LANGUAGE_VERSION"_s, makeString(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))));

#if USE(OPENGL_ES)
    addTableRow(hardwareAccelerationObject, "GL_EXTENSIONS"_s, makeString(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS))));
#else
    StringBuilder extensionsBuilder;
    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    for (GLint i = 0; i < numExtensions; ++i) {
        if (i)
            extensionsBuilder.append(' ');
        extensionsBuilder.append(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i)));
    }
    addTableRow(hardwareAccelerationObject, "GL_EXTENSIONS"_s, extensionsBuilder.toString());
#endif

#if USE(GLX)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11 && !isEGL) {
        auto* x11Display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();
        addTableRow(hardwareAccelerationObject, "GLX_VERSION"_s, makeString(glXGetClientString(x11Display, GLX_VERSION)));
        addTableRow(hardwareAccelerationObject, "GLX_VENDOR"_s, makeString(glXGetClientString(x11Display, GLX_VENDOR)));
        addTableRow(hardwareAccelerationObject, "GLX_EXTENSIONS"_s, makeString(glXGetClientString(x11Display, GLX_EXTENSIONS)));
    }
#endif

#if USE(EGL)
    if (isEGL) {
        auto eglDisplay = PlatformDisplay::sharedDisplay().eglDisplay();
        addTableRow(hardwareAccelerationObject, "EGL_VERSION"_s, makeString(eglQueryString(eglDisplay, EGL_VERSION)));
        addTableRow(hardwareAccelerationObject, "EGL_VENDOR"_s, makeString(eglQueryString(eglDisplay, EGL_VENDOR)));
        addTableRow(hardwareAccelerationObject, "EGL_EXTENSIONS"_s, makeString(eglQueryString(nullptr, EGL_EXTENSIONS), ' ', eglQueryString(eglDisplay, EGL_EXTENSIONS)));
    }
#endif
#endif // USE(EGL) || USE(GLX)

    stopTable();
    jsonObject->setObject("Hardware Acceleration Information"_s, WTFMove(hardwareAccelerationObject));

    auto infoAsString = jsonObject->toJSONString();
    g_string_append_printf(html, "<script>function copyAsJSON() { "
        "var textArea = document.createElement('textarea');"
        "textArea.value = JSON.stringify(%s, null, 4);"
        "document.body.appendChild(textArea);"
        "textArea.focus();"
        "textArea.select();"
        "document.execCommand('copy');"
        "document.body.removeChild(textArea);"
        "}</script>", infoAsString.utf8().data());

    g_string_append_printf(html, "<script>function sendToConsole() { "
        "console.log(JSON.stringify(%s, null, 4));"
        "}</script>", infoAsString.utf8().data());

    g_string_append(html, "</head><body>");
#if PLATFORM(GTK)
    g_string_append(html, "<button onclick=\"copyAsJSON()\">Copy to clipboard</button>");
#else
    // WPE doesn't seem to pass clipboard data yet.
    g_string_append(html, "<button onclick=\"sendToConsole()\">Send to JS console</button>");
#endif

    g_string_append_printf(html, "%s</body></html>", tablesBuilder.toString().utf8().data());
    gsize streamLength = html->len;
    GRefPtr<GInputStream> stream = adoptGRef(g_memory_input_stream_new_from_data(g_string_free(html, FALSE), streamLength, g_free));
    webkit_uri_scheme_request_finish(request, stream.get(), streamLength, "text/html");
}

} // namespace WebKit
