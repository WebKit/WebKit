/*
 * Copyright (C) 2019 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include "APIPageConfiguration.h"
#include "BuildRevision.h"
#include "DRMMainDevice.h"
#include "DisplayVBlankMonitor.h"
#include "RenderProcessInfo.h"
#include "RendererBufferTransportMode.h"
#include "WebKitError.h"
#include "WebKitURISchemeRequestPrivate.h"
#include "WebKitVersion.h"
#include "WebKitWebViewPrivate.h"
#include "WebPageMessages.h"
#include "WebProcessPool.h"
#include <WebCore/FloatRect.h>
#include <WebCore/GLContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/PlatformScreen.h>
#include <cstdlib>
#include <epoxy/gl.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/WorkQueue.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if OS(UNIX)
#include <sys/utsname.h>
#endif

#if PLATFORM(GTK)
#include "AcceleratedBackingStore.h"
#include "Display.h"
#include <gtk/gtk.h>
#endif

#if PLATFORM(WPE)
#include "WPEUtilities.h"
#if ENABLE(WPE_PLATFORM)
#include "DisplayVBlankMonitorWPE.h"
#include <wpe/wpe-platform.h>
#endif
#endif

#if USE(LIBDRM)
#include <xf86drm.h>
#endif

#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#endif

#if USE(GSTREAMER)
#include <gst/gst.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebKitProtocolHandler);

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
        auto& page = webkitURISchemeRequestGetWebPage(request);
        protect(page.legacyMainFrameProcess())->sendWithAsyncReply(Messages::WebPage::GetRenderProcessInfo(), [this, request = GRefPtr<WebKitURISchemeRequest>(request)](RenderProcessInfo&& info) {
            handleGPU(request.get(), WTF::move(info));
        }, page.webPageIDInMainFrameProcess());
        return;
    }

    GUniquePtr<GError> error(g_error_new_literal(WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI, "Not found"));
        webkit_uri_scheme_request_finish_error(request, error.get());
}

static inline ASCIILiteral webkitPortName()
{
#if PLATFORM(GTK)
    return "WebKitGTK"_s;
#elif PLATFORM(WPE)
    return "WPE WebKit"_s;
#endif
    RELEASE_ASSERT_NOT_REACHED();
}

static ASCIILiteral hardwareAccelerationPolicy(WebKitURISchemeRequest* request)
{
#if PLATFORM(WPE)
    return "always"_s;
#elif PLATFORM(GTK)
    auto* webView = webkit_uri_scheme_request_get_web_view(request);
    ASSERT(webView);

    switch (webkit_settings_get_hardware_acceleration_policy(webkit_web_view_get_settings(webView))) {
    case WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER:
        return "never"_s;
    case WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS:
        return "always"_s;
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

static bool canvasAccelerationEnabled(WebKitURISchemeRequest* request)
{
    auto* webView = webkit_uri_scheme_request_get_web_view(request);
    ASSERT(webView);
    return webkit_settings_get_enable_2d_canvas_acceleration(webkit_web_view_get_settings(webView));
}

static bool uiProcessContextIsEGL()
{
#if PLATFORM(GTK)
    return Display::singleton().glDisplayIsSharedWithGtk();
#else
    return true;
#endif
}

static const char* openGLAPI()
{
    if (epoxy_is_desktop_gl())
        return "OpenGL (libepoxy)";
    return "OpenGL ES 2 (libepoxy)";
}

#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
static String dmabufRendererWithSupportedBuffers()
{
    StringBuilder buffers;
    buffers.append("DMABuf (Supported buffers: "_s);

#if PLATFORM(GTK)
    auto mode = AcceleratedBackingStore::rendererBufferTransportMode();
#else
    OptionSet<RendererBufferTransportMode> mode;
    if (wpe_display_get_drm_device(wpe_display_get_primary()))
        mode.add(RendererBufferTransportMode::Hardware);
    mode.add(RendererBufferTransportMode::SharedMemory);
#endif

    if (mode.contains(RendererBufferTransportMode::Hardware))
        buffers.append("Hardware"_s);
    if (mode.contains(RendererBufferTransportMode::SharedMemory)) {
        if (mode.contains(RendererBufferTransportMode::Hardware))
            buffers.append(", "_s);
        buffers.append("Shared Memory"_s);
    }

    buffers.append(')');
    return buffers.toString();
}

#if USE(LIBDRM)

// Base on function 'drmGetFormatName' from 'https://gitlab.freedesktop.org/mesa/drm/-/blob/main/xf86drm.c'.
static String webkitDrmGetFormatName(uint32_t format)
{
    bool isBigEndian = (format & (1U << 31));
    format &= ~(1U<<31);

    // If format is DRM_FORMAT_INVALID.
    if (!format)
        return "INVALID"_s;

    std::span<char> buffer;
    CString code = CString::newUninitialized(4, buffer);
    buffer[0] = static_cast<char>((format >> 0) & 0xFF);
    buffer[1] = static_cast<char>((format >> 8) & 0xFF);
    buffer[2] = static_cast<char>((format >> 16) & 0xFF);
    buffer[3] = static_cast<char>((format >> 24) & 0xFF);

    // Trim spaces at the end.
    for (size_t i = 3; i > 0 && buffer[i] == ' '; --i)
        buffer[i] = '\0';

    return makeString(code, isBigEndian ? "_BE"_s : ""_s);
}

static String webkitDrmGetModifierName(uint64_t modifier)
{
#if HAVE(DRM_GET_FORMAT_MODIFIER_VENDOR) && HAVE(DRM_GET_FORMAT_MODIFIER_NAME)
    std::unique_ptr<char, decltype(free)*> modifierVendor(drmGetFormatModifierVendor(modifier), free);
    std::unique_ptr<char, decltype(free)*> modifierName(drmGetFormatModifierName(modifier), free);
    return makeString(String::fromUTF8(modifierVendor.get()), "_"_s, String::fromUTF8(modifierName.get()));
#else
    return { };
#endif
}

static String modifierListToString(const Vector<uint64_t, 1>& modifiers)
{
    if (modifiers.isEmpty())
        return { };

#if HAVE(DRM_GET_FORMAT_MODIFIER_VENDOR) && HAVE(DRM_GET_FORMAT_MODIFIER_NAME)
    StringBuilder builder;
    for (auto modifier : modifiers) {
        if (!builder.isEmpty())
            builder.append(", "_s);
        builder.append(webkitDrmGetModifierName(modifier));
    }
    return makeString(" ("_s, builder.toString(), ")"_s);
#else
    return { };
#endif
}

static String renderBufferDescription(WebKitURISchemeRequest* request)
{
    StringBuilder bufferDescription;
    auto description = webkitWebViewGetRendererBufferDescription(webkit_uri_scheme_request_get_web_view(request));
    if (description.fourcc) {
        auto formatName = webkitDrmGetFormatName(description.fourcc);
        switch (description.type) {
        case RendererBufferDescription::Type::DMABuf: {
            auto modifierName = webkitDrmGetModifierName(description.modifier);
            if (!modifierName.isNull())
                bufferDescription.append("DMA-BUF: "_s, formatName, " ("_s, modifierName, ")"_s);
            else
                bufferDescription.append("Unknown"_s);
            break;
        }
        case RendererBufferDescription::Type::SharedMemory:
            bufferDescription.append("Shared Memory: "_s, formatName);
            break;
#if OS(ANDROID)
        case RendererBufferDescription::Type::AHardwareBuffer:
            bufferDescription.append("AHardwareBuffer: "_s, formatName);
            break;
#endif
        }
        switch (description.usage) {
        case RendererBufferFormat::Usage::Rendering:
            bufferDescription.append(" [Rendering]"_s);
            break;
        case RendererBufferFormat::Usage::Scanout:
            bufferDescription.append(" [Scanout]"_s);
            break;
        case RendererBufferFormat::Usage::Mapping:
            bufferDescription.append(" [Mapping]"_s);
            break;
        }
    } else
        bufferDescription.append("Unknown"_s);

    return bufferDescription.toString();
}

#if USE(GBM)
static String preferredBufferFormats(WebKitURISchemeRequest* request, JSON::Array& jsonArray)
{
    auto& page = webkitURISchemeRequestGetWebPage(request);
    auto formats = page.preferredBufferFormats();
    StringBuilder builder;
    builder.append("<ul>"_s);
    for (const auto& tranche : formats) {
        auto jsonObject = JSON::Object::create();
        builder.append("<li>Formats for "_s);
        switch (tranche.usage) {
        case RendererBufferFormat::Usage::Rendering:
            builder.append("<b>rendering</b> using device <i>"_s, !tranche.drmDevice.renderNode.isNull() ? tranche.drmDevice.renderNode : tranche.drmDevice.primaryNode, "</i>"_s);
            jsonObject->setString("Usage"_s, "Rendering"_s);
            jsonObject->setString("Device"_s, String::fromUTF8(!tranche.drmDevice.renderNode.isNull() ? tranche.drmDevice.renderNode.span() : tranche.drmDevice.primaryNode.span()));
            break;
        case RendererBufferFormat::Usage::Scanout:
            builder.append("<b>scanout</b> using device <i>"_s, tranche.drmDevice.primaryNode, "</i>"_s);
            jsonObject->setString("Usage"_s, "Scanout"_s);
            jsonObject->setString("Device"_s, String::fromUTF8(tranche.drmDevice.primaryNode.span()));
            break;
        case RendererBufferFormat::Usage::Mapping:
            builder.append("<b>mapping</b> using device <i>"_s, tranche.drmDevice.primaryNode, "</i>"_s);
            jsonObject->setString("Usage"_s, "Mapping"_s);
            jsonObject->setString("Device"_s, String::fromUTF8(tranche.drmDevice.primaryNode.span()));
            break;
        }
        builder.append("<br>"_s);
        auto jsonFormats = JSON::Array::create();
        StringBuilder formatsBuilder;
        for (const auto& format : tranche.formats) {
            StringBuilder jsonStringBuilder;
            if (!formatsBuilder.isEmpty())
                formatsBuilder.append("<br>"_s);
            auto formatName = webkitDrmGetFormatName(format.fourcc);
            formatsBuilder.append("<b>"_s, formatName, "</b>"_s);
            jsonStringBuilder.append(formatName);
            auto modifiers = modifierListToString(format.modifiers);
            if (!modifiers.isNull()) {
                formatsBuilder.append(modifiers);
                jsonStringBuilder.append(modifiers);
            }
            jsonFormats->pushString(jsonStringBuilder.toString());
        }
        builder.append(formatsBuilder.toString());
        jsonObject->setArray("Formats"_s, WTF::move(jsonFormats));
        jsonArray.pushObject(WTF::move(jsonObject));
    }
    builder.append("</ul>"_s);
    return builder.toString();
}
#endif // USE(GBM)
#endif // USE(LIBDRM)
#endif // PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))

static String vblankMonitorType(const DisplayVBlankMonitor& monitor)
{
#if ENABLE(WPE_PLATFORM)
    if (monitor.type() == DisplayVBlankMonitor::Type::Wpe) {
        const auto& wpeMonitor = *static_cast<const DisplayVBlankMonitorWPE*>(&monitor);
        return makeString("WPE ("_s, String::fromUTF8(G_OBJECT_TYPE_NAME(wpeMonitor.observer())), ')');
    }
#endif

    return monitor.type() == DisplayVBlankMonitor::Type::Timer ? "Timer"_s : "DRM"_s;
}

static String threadedRenderingInfo(const RenderProcessInfo& info)
{
    if (!info.cpuPaintingThreadsCount && !info.gpuPaintingThreadsCount)
        return "Disabled"_s;

    if (info.cpuPaintingThreadsCount)
        return makeString("CPU ("_s, info.cpuPaintingThreadsCount, " threads)"_s);

    ASSERT(info.gpuPaintingThreadsCount);
    return makeString("GPU ("_s, info.gpuPaintingThreadsCount, " threads)"_s);
}

#if USE(LIBDRM)
static String supportedBufferFormats(const RenderProcessInfo& info, JSON::Array& jsonArray)
{
    StringBuilder builder;
#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
    for (const auto& format : info.supportedBufferFormats) {
        StringBuilder jsonStringBuilder;
        auto formatName = webkitDrmGetFormatName(format.fourcc);
        if (!builder.isEmpty())
            builder.append("<br>"_s);
        builder.append("<b>"_s, formatName, "</b>"_s);
        jsonStringBuilder.append(formatName);
        auto modifiers = modifierListToString(format.modifiers);
        if (!modifiers.isNull()) {
            builder.append(modifiers);
            jsonStringBuilder.append(modifiers);
        }
        jsonArray.pushString(jsonStringBuilder.toString());
    }
#endif
    return builder.toString();
}
#endif

static String viewActivityState(WebKitURISchemeRequest* request)
{
    auto& page = webkitURISchemeRequestGetWebPage(request);
    Vector<String, 4> state;
    if (page.isInWindow())
        state.append("in window"_s);
    if (page.isViewVisible())
        state.append("visible"_s);
    if (page.isViewFocused())
        state.append("focused"_s);
    if (page.isViewWindowActive())
        state.append("active"_s);
    return makeStringByJoining(state.span(), ", "_s);
}

#if PLATFORM(GTK)
static String toplevelState(WebKitURISchemeRequest* request)
{
#if USE(GTK4)
    auto* webView = webkit_uri_scheme_request_get_web_view(request);
    auto* root = gtk_widget_get_root(GTK_WIDGET(webView));
    if (!root)
        return { };

    auto* surface = gtk_native_get_surface(GTK_NATIVE(root));
    if (!surface || !GDK_IS_TOPLEVEL(surface))
        return { };

    auto state = gdk_toplevel_get_state(GDK_TOPLEVEL(surface));
    if (state & GDK_TOPLEVEL_STATE_FULLSCREEN)
        return "fullscreen"_s;
    if (state & GDK_TOPLEVEL_STATE_MAXIMIZED)
        return "maximized"_s;
    return "normal"_s;
#else
    return { };
#endif
}
#elif PLATFORM(WPE)
static String toplevelState(WebKitURISchemeRequest* request)
{
#if ENABLE(WPE_PLATFORM)
    if (!WKWPE::isUsingWPEPlatformAPI())
        return { };

    auto* webView = webkit_uri_scheme_request_get_web_view(request);
    auto* view = webkit_web_view_get_wpe_view(webView);
    if (!view)
        return { };

    auto state = wpe_view_get_toplevel_state(view);
    if (state & WPE_TOPLEVEL_STATE_FULLSCREEN)
        return "fullscreen"_s;
    if (state & WPE_TOPLEVEL_STATE_MAXIMIZED)
        return "maximized"_s;
    return "normal"_s;
#else
    return { };
#endif
}
#endif

static String prettyPrintJSON(const String& jsonString)
{
    StringBuilder result;
    result.reserveCapacity(jsonString.length() + 128);
    int indentLevel = 0;
    bool inQuotes = false;
    bool escape = false;
    // 4 spaces per identation level
    constexpr auto identSpaceLevel = "    "_s;

    for (auto ch : StringView(jsonString).codePoints()) {
        switch (ch) {
        case '\"':
            if (!escape)
                inQuotes = !inQuotes;
            result.append(ch);
            break;
        case '{':
        case '[':
            result.append(ch);
            if (!inQuotes) {
                result.append('\n');
                indentLevel++;
                for (int i = 0; i < indentLevel; ++i)
                    result.append(identSpaceLevel);
            }
            break;
        case '}':
        case ']':
            if (!inQuotes) {
                result.append('\n');
                indentLevel--;
                for (int i = 0; i < indentLevel; ++i)
                    result.append(identSpaceLevel);
            }
            result.append(ch);
            break;
        case ',':
            result.append(ch);
            if (!inQuotes) {
                result.append('\n');
                for (int i = 0; i < indentLevel; ++i)
                    result.append(identSpaceLevel);
            }
            break;
        case ':':
            result.append(ch);
            if (!inQuotes)
                result.append(' ');
            break;
        case '\\':
            result.append(ch);
            escape = !escape;
            break;
        default:
            result.append(ch);
            escape = false;
            break;
        }
    }
    return result.toString();
}

static unsigned refreshParameter(const URL& url)
{
    auto parameters = queryParameters(url);
    for (const auto& parameter : parameters) {
        if (parameter.key == "refresh"_s)
            return parseInteger<unsigned>(parameter.value).value_or(0);
    }
    return 0;
}

void WebKitProtocolHandler::handleGPU(WebKitURISchemeRequest* request, RenderProcessInfo&& info)
{
    URL requestURL = URL(String::fromLatin1(webkit_uri_scheme_request_get_uri(request)));

    StringBuilder htmlBuilder;
    htmlBuilder.append("<html><head><title>GPU information</title>"
        "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"_s);
    if (auto refresh = refreshParameter(requestURL))
        htmlBuilder.append("<meta http-equiv=\"refresh\" content=\""_s, refresh, "\" />"_s);
    htmlBuilder.append("<style>"
        "  h1 { color: #babdb6; text-shadow: 0 1px 0 white; margin-bottom: 0; }"
        "  html { font-family: -webkit-system-font; font-size: 11pt; color: #2e3436; padding: 20px 20px 0 20px; background-color: #f6f6f4; "
        "         background-image: -webkit-gradient(linear, left top, left bottom, color-stop(0, #eeeeec), color-stop(1, #f6f6f4));"
        "         background-size: 100% 5em; background-repeat: no-repeat; }"
        "  table { width: 100%; border-collapse: collapse; }"
        "  table, td { border: 1px solid #d3d7cf; border-left: none; border-right: none; }"
        "  p { margin-bottom: 30px; }"
        "  table tr > td:first-child { width: 25% }"
        "  td { padding: 15px; }"
        "  td.data { width: 200px; }"
        "  .titlename { font-weight: bold; }"
        "</style>"_s);

    StringBuilder tablesBuilder;

    auto startTable = [&](auto header) {
        tablesBuilder.append("<h1>"_s, header, "</h1><table>"_s);
    };

    auto addTableRow = [&](auto& jsonObject, auto key, auto&& value, RefPtr<JSON::Value>&& jsonValue = nullptr) {
        tablesBuilder.append("<tbody><tr><td><div class=\"titlename\">"_s, key, "</div></td><td>"_s, value, "</td></tr></tbody>"_s);
        if (jsonValue)
            jsonObject->setValue(key, jsonValue.releaseNonNull());
        else
            jsonObject->setString(key, value);
    };

    auto stopTable = [&] {
        tablesBuilder.append("</table>"_s);
    };

    auto addEGLInfo = [&](auto& jsonObject) {
        addTableRow(jsonObject, "GL_RENDERER"_s, String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_RENDERER))));
        addTableRow(jsonObject, "GL_VENDOR"_s, String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_VENDOR))));
        addTableRow(jsonObject, "GL_VERSION"_s, String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
        addTableRow(jsonObject, "GL_SHADING_LANGUAGE_VERSION"_s, String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))));

        switch (eglQueryAPI()) {
        case EGL_OPENGL_ES_API:
            addTableRow(jsonObject, "GL_EXTENSIONS"_s, String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS))));
            break;
        case EGL_OPENGL_API: {
            StringBuilder extensionsBuilder;
            GLint numExtensions = 0;
            glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
            for (GLint i = 0; i < numExtensions; ++i) {
                if (i)
                    extensionsBuilder.append(' ');
                extensionsBuilder.append(unsafeSpan(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i))));
            }
            addTableRow(jsonObject, "GL_EXTENSIONS"_s, extensionsBuilder.toString());
            break;
        }
        }

        auto eglDisplay = eglGetCurrentDisplay();
        addTableRow(jsonObject, "EGL_VERSION"_s, String::fromUTF8(eglQueryString(eglDisplay, EGL_VERSION)));
        addTableRow(jsonObject, "EGL_VENDOR"_s, String::fromUTF8(eglQueryString(eglDisplay, EGL_VENDOR)));
        addTableRow(jsonObject, "EGL_EXTENSIONS"_s, makeString(unsafeSpan(eglQueryString(nullptr, EGL_EXTENSIONS)), ' ', unsafeSpan(eglQueryString(eglDisplay, EGL_EXTENSIONS))));
    };

    auto jsonObject = JSON::Object::create();

    startTable("Version Information"_s);
    auto versionObject = JSON::Object::create();
    addTableRow(versionObject, "WebKit version"_s, makeString(webkitPortName(), ' ', WEBKIT_MAJOR_VERSION, '.', WEBKIT_MINOR_VERSION, '.', WEBKIT_MICRO_VERSION, " ("_s, unsafeSpan(BUILD_REVISION), ')'));

#if OS(UNIX)
    struct utsname osName;
    uname(&osName);
    addTableRow(versionObject, "Operating system"_s, makeString(unsafeSpan(osName.sysname), ' ', unsafeSpan(osName.release), ' ', unsafeSpan(osName.version), ' ', unsafeSpan(osName.machine)));
#endif

    const char* desktopName = g_getenv("XDG_CURRENT_DESKTOP");
    addTableRow(versionObject, "Desktop"_s, (desktopName && *desktopName) ? String::fromUTF8(desktopName) : "Unknown"_s);

#if USE(GSTREAMER)
    GUniquePtr<char> gstVersion(gst_version_string());
    addTableRow(versionObject, "GStreamer version"_s, makeString(GST_VERSION_MAJOR, '.', GST_VERSION_MINOR, '.', GST_VERSION_MICRO, " (build) "_s, unsafeSpan(gstVersion.get()), " (runtime)"_s));
#endif

#if PLATFORM(GTK)
    addTableRow(versionObject, "GTK version"_s, makeString(GTK_MAJOR_VERSION, '.', GTK_MINOR_VERSION, '.', GTK_MICRO_VERSION, " (build) "_s, gtk_get_major_version(), '.', gtk_get_minor_version(), '.', gtk_get_micro_version(), " (runtime)"_s));
#endif

#if PLATFORM(WPE)
    bool usingWPEPlatformAPI = WKWPE::isUsingWPEPlatformAPI();
#if USE(LIBWPE)
    if (!usingWPEPlatformAPI) {
        addTableRow(versionObject, "WPE version"_s, makeString(WPE_MAJOR_VERSION, '.', WPE_MINOR_VERSION, '.', WPE_MICRO_VERSION, " (build) "_s, wpe_get_major_version(), '.', wpe_get_minor_version(), '.', wpe_get_micro_version(), " (runtime)"_s));
        addTableRow(versionObject, "WPE backend"_s, String::fromUTF8(wpe_loader_get_loaded_implementation_library_name()));
    }
#endif
#endif

    stopTable();
    jsonObject->setObject("Version Information"_s, WTF::move(versionObject));

    auto displayObject = JSON::Object::create();
    startTable("Display Information"_s);

    Ref page = webkitURISchemeRequestGetWebPage(request);
    auto displayID = page->displayID();
    addTableRow(displayObject, "Identifier"_s, String::number(displayID.value_or(0)));

#if PLATFORM(GTK)
    StringBuilder typeStringBuilder;
    if (Display::singleton().isWayland())
        typeStringBuilder.append("Wayland"_s);
    else if (Display::singleton().isX11())
        typeStringBuilder.append("X11"_s);
    addTableRow(displayObject, "Type"_s, !typeStringBuilder.isEmpty() ? typeStringBuilder.toString() : "Unknown"_s);
#endif // PLATFORM(GTK)

    auto policy = hardwareAccelerationPolicy(request);

    auto rect = IntRect(screenRect(nullptr));
    addTableRow(displayObject, "Screen geometry"_s, makeString(rect.x(), ',', rect.y(), ' ', rect.width(), 'x', rect.height()));

    rect = IntRect(screenAvailableRect(nullptr));
    addTableRow(displayObject, "Screen work area"_s, makeString(rect.x(), ',', rect.y(), ' ', rect.width(), 'x', rect.height()));
    addTableRow(displayObject, "Device scale"_s, String::number(page->deviceScaleFactor()));
    addTableRow(displayObject, "Depth"_s, String::number(screenDepth(nullptr)));
    addTableRow(displayObject, "Bits per color component"_s, String::number(screenDepthPerComponent(nullptr)));
    addTableRow(displayObject, "Font Scaling DPI"_s, String::number(WebCore::fontDPI()));
#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
    addTableRow(displayObject, "Screen DPI"_s, String::number(screenDPI(displayID.value_or(primaryScreenDisplayID()))));
#endif

    if (displayID) {
        if (auto* displayLink = page->configuration().processPool().displayLinks().existingDisplayLinkForDisplay(*displayID)) {
            auto& vblankMonitor = displayLink->vblankMonitor();
            addTableRow(displayObject, "VBlank type"_s, vblankMonitorType(vblankMonitor));
            addTableRow(displayObject, "VBlank refresh rate"_s, makeString(vblankMonitor.refreshRate(), "Hz"_s));
        }
    }

#if USE(GBM)
    if (policy != "never"_s) {
        auto drmDevice = drmMainDevice();
        if (!drmDevice.isNull()) {
            addTableRow(displayObject, "DRM Primary Node"_s, String::fromUTF8(drmDevice.primaryNode.span()));
            if (!drmDevice.renderNode.isNull())
                addTableRow(displayObject, "DRM Render Node"_s, String::fromUTF8(drmDevice.renderNode.span()));
        }
    }
#endif

    stopTable();
    jsonObject->setObject("Display Information"_s, WTF::move(displayObject));

    auto viewObject = JSON::Object::create();
    startTable("View Information"_s);

    addTableRow(viewObject, "Size"_s, makeString(page->viewSize().width(), 'x', page->viewSize().height()));
    addTableRow(viewObject, "State"_s, viewActivityState(request));
    auto state = toplevelState(request);
    if (!state.isNull())
        addTableRow(viewObject, "Toplevel state"_s, state);

    stopTable();
    jsonObject->setObject("View Information"_s, WTF::move(viewObject));

    auto hardwareAccelerationObject = JSON::Object::create();
    startTable("Hardware Acceleration Information"_s);
    addTableRow(hardwareAccelerationObject, "Policy"_s, policy);

#if ENABLE(WEBGL)
    addTableRow(hardwareAccelerationObject, "WebGL enabled"_s, webGLEnabled(request) ? "Yes"_s : "No"_s);
#endif

    addTableRow(hardwareAccelerationObject, "2D canvas"_s, canvasAccelerationEnabled(request) ? "Accelerated"_s : "Unaccelerated"_s);

    if (policy != "never"_s) {
        addTableRow(hardwareAccelerationObject, "API"_s, String::fromUTF8(openGLAPI()));
#if PLATFORM(GTK)
        bool showBuffersInfo = true;
#elif PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
        bool showBuffersInfo = usingWPEPlatformAPI;
#else
        bool showBuffersInfo = false;
#endif
        if (showBuffersInfo) {
#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
            addTableRow(hardwareAccelerationObject, "Renderer"_s, dmabufRendererWithSupportedBuffers());
#if USE(LIBDRM)
#if USE(GBM)
            auto jsonFormats = JSON::Array::create();
            auto formatsString = preferredBufferFormats(request, jsonFormats.get());
            addTableRow(hardwareAccelerationObject, "Preferred buffer formats"_s, formatsString, WTF::move(jsonFormats));
#endif
            addTableRow(hardwareAccelerationObject, "Buffer format"_s, renderBufferDescription(request));
#endif // USE(LIBDRM)
#endif // PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
        }

        addTableRow(hardwareAccelerationObject, "Native interface"_s, uiProcessContextIsEGL() ? "EGL"_s : "None"_s);

        if (uiProcessContextIsEGL() && eglGetCurrentContext() != EGL_NO_CONTEXT)
            addEGLInfo(hardwareAccelerationObject);
    } else {
#if PLATFORM(GTK)
        addTableRow(hardwareAccelerationObject, "Buffer format"_s, renderBufferDescription(request));
#endif
    }

    stopTable();
    jsonObject->setObject("Hardware Acceleration Information"_s, WTF::move(hardwareAccelerationObject));

    if (policy != "never"_s && !info.platform.isEmpty()) {
        auto hardwareAccelerationObject = JSON::Object::create();
        startTable("Hardware Acceleration Information (Render Process)"_s);

        addTableRow(hardwareAccelerationObject, "Platform"_s, info.platform);

        if (!info.drmVersion.isEmpty())
            addTableRow(hardwareAccelerationObject, "DRM version"_s, info.drmVersion);

        addTableRow(hardwareAccelerationObject, "Threaded rendering"_s, threadedRenderingInfo(info));
        addTableRow(hardwareAccelerationObject, "MSAA"_s, info.msaaSampleCount ? makeString(info.msaaSampleCount, " samples"_s) : String("Disabled"_s));

#if USE(LIBDRM)
        if (!info.supportedBufferFormats.isEmpty()) {
            auto jsonFormats = JSON::Array::create();
            auto formatsString = supportedBufferFormats(info, jsonFormats.get());
            addTableRow(hardwareAccelerationObject, "Supported buffers"_s, formatsString, WTF::move(jsonFormats));
        }
#endif

        addTableRow(hardwareAccelerationObject, "GL_RENDERER"_s, info.glRenderer);
        addTableRow(hardwareAccelerationObject, "GL_VENDOR"_s, info.glVendor);
        addTableRow(hardwareAccelerationObject, "GL_VERSION"_s, info.glVersion);
        addTableRow(hardwareAccelerationObject, "GL_SHADING_LANGUAGE_VERSION"_s, info.glShadingVersion);
        addTableRow(hardwareAccelerationObject, "GL_EXTENSIONS"_s, info.glExtensions);
        addTableRow(hardwareAccelerationObject, "EGL_VERSION"_s, info.eglVersion);
        addTableRow(hardwareAccelerationObject, "EGL_VENDOR"_s, info.eglVendor);
        addTableRow(hardwareAccelerationObject, "EGL_EXTENSIONS"_s, info.eglExtensions);

        stopTable();
        jsonObject->setObject("Hardware Acceleration Information (Render process)"_s, WTF::move(hardwareAccelerationObject));
    }

    auto infoAsString = jsonObject->toJSONString();
    htmlBuilder.append("<script>function copyAsJSON() { "
        "var textArea = document.createElement('textarea');"
        "textArea.value = JSON.stringify("_s, infoAsString, ", null, 4);"_s,
        "document.body.appendChild(textArea);"
        "textArea.focus();"
        "textArea.select();"
        "document.execCommand('copy');"
        "document.body.removeChild(textArea);"
        "}</script></head><body>"_s);

#if PLATFORM(GTK)
    // WPE doesn't seem to pass clipboard data yet.
    htmlBuilder.append("<button onclick=\"copyAsJSON()\">Copy to clipboard</button>"_s);
#endif
    htmlBuilder.append("<button onclick=\"window.location.href='webkit://gpu/stdout'\">Print in stdout</button>"_s);

    htmlBuilder.append(tablesBuilder.toString(), "</body></html>"_s);

    auto html = htmlBuilder.toString().utf8();
    gsize streamLength = html.length();
    GRefPtr<GInputStream> stream = adoptGRef(g_memory_input_stream_new_from_data(g_strdup(html.data()), streamLength, g_free));
    webkit_uri_scheme_request_finish(request, stream.get(), streamLength, "text/html");

    if (requestURL.path() == "/stdout"_s)
        WTFLogAlways("GPU information\n%s", prettyPrintJSON(infoAsString).utf8().data());
}

} // namespace WebKit

