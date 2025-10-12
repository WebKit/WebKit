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
#include "RendererBufferTransportMode.h"
#include "WebKitError.h"
#include "WebKitURISchemeRequestPrivate.h"
#include "WebKitVersion.h"
#include "WebKitWebViewPrivate.h"
#include "WebProcessPool.h"
#include <WebCore/FloatRect.h>
#include <WebCore/GLContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/PlatformDisplaySurfaceless.h>
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
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if OS(UNIX)
#include <sys/utsname.h>
#endif

#if USE(CAIRO)
#include <cairo.h>
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

#if USE(GBM)
#include <WebCore/PlatformDisplayGBM.h>
#include <gbm.h>
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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GTK/WPE port

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
        handleGPU(request);
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
#if !USE(GTK4)
    case WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND:
        return "on demand"_s;
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

#if USE(SKIA)
static bool canvasAccelerationEnabled(WebKitURISchemeRequest* request)
{
    auto* webView = webkit_uri_scheme_request_get_web_view(request);
    ASSERT(webView);
    return webkit_settings_get_enable_2d_canvas_acceleration(webkit_web_view_get_settings(webView));
}
#endif

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

// Cherry-pick function 'drmGetFormatName' from 'https://gitlab.freedesktop.org/mesa/drm/-/blob/main/xf86drm.c'.
// Function is only available since version '2.4.113'. Debian 11 ships '2.4.104'.
// FIXME: Remove when Debian 11 support ends.
static char* webkitDrmGetFormatName(uint32_t format)
{
    char* str;
    char code[5];
    const char* be;
    size_t strSize, i;

    // Is format big endian?
    be = (format & (1U<<31)) ? "_BE" : "";
    format &= ~(1U<<31);

    // If format is DRM_FORMAT_INVALID.
    if (!format)
        return strdup("INVALID");

    code[0] = (char) ((format >> 0) & 0xFF);
    code[1] = (char) ((format >> 8) & 0xFF);
    code[2] = (char) ((format >> 16) & 0xFF);
    code[3] = (char) ((format >> 24) & 0xFF);
    code[4] = '\0';

    // Trim spaces at the end.
    for (i = 3; i > 0 && code[i] == ' '; i--)
        code[i] = '\0';

    strSize = strlen(code) + strlen(be) + 1;
    str = static_cast<char*>(malloc(strSize));
    if (!str)
        return nullptr;

    snprintf(str, strSize, "%s%s", code, be);

    return str;
}

static String renderBufferDescription(WebKitURISchemeRequest* request)
{
    StringBuilder bufferDescription;
    auto description = webkitWebViewGetRendererBufferDescription(webkit_uri_scheme_request_get_web_view(request));
    if (description.fourcc) {
        auto* formatName = webkitDrmGetFormatName(description.fourcc);
        switch (description.type) {
        case RendererBufferDescription::Type::DMABuf: {
#if HAVE(DRM_GET_FORMAT_MODIFIER_VENDOR) && HAVE(DRM_GET_FORMAT_MODIFIER_NAME)
            auto* modifierVendor = drmGetFormatModifierVendor(description.modifier);
            auto* modifierName = drmGetFormatModifierName(description.modifier);
            bufferDescription.append("DMA-BUF: "_s, String::fromUTF8(formatName), " ("_s, String::fromUTF8(modifierVendor), "_"_s, String::fromUTF8(modifierName), ")"_s);
            free(modifierVendor);
            free(modifierName);
#else
            bufferDescription.append("Unknown"_s);
#endif
            break;
        }
        case RendererBufferDescription::Type::SharedMemory:
            bufferDescription.append("Shared Memory: "_s, String::fromUTF8(formatName));
            break;
        }
        free(formatName);
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
#endif
#endif

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

void WebKitProtocolHandler::handleGPU(WebKitURISchemeRequest* request)
{
    URL requestURL = URL(String::fromLatin1(webkit_uri_scheme_request_get_uri(request)));
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

#if USE(CAIRO)
    addTableRow(versionObject, "Cairo version"_s, makeString(unsafeSpan(CAIRO_VERSION_STRING), " (build) "_s, unsafeSpan(cairo_version_string()), " (runtime)"_s));
#endif

#if USE(GSTREAMER)
    GUniquePtr<char> gstVersion(gst_version_string());
    addTableRow(versionObject, "GStreamer version"_s, makeString(GST_VERSION_MAJOR, '.', GST_VERSION_MINOR, '.', GST_VERSION_MICRO, " (build) "_s, unsafeSpan(gstVersion.get()), " (runtime)"_s));
#endif

#if PLATFORM(GTK)
    addTableRow(versionObject, "GTK version"_s, makeString(GTK_MAJOR_VERSION, '.', GTK_MINOR_VERSION, '.', GTK_MICRO_VERSION, " (build) "_s, gtk_get_major_version(), '.', gtk_get_minor_version(), '.', gtk_get_micro_version(), " (runtime)"_s));

    bool usingDMABufRenderer = AcceleratedBackingStore::checkRequirements();
#endif

#if PLATFORM(WPE)
    bool usingWPEPlatformAPI = WKWPE::isUsingWPEPlatformAPI();
    if (!usingWPEPlatformAPI) {
        addTableRow(versionObject, "WPE version"_s, makeString(WPE_MAJOR_VERSION, '.', WPE_MINOR_VERSION, '.', WPE_MICRO_VERSION, " (build) "_s, wpe_get_major_version(), '.', wpe_get_minor_version(), '.', wpe_get_micro_version(), " (runtime)"_s));
        addTableRow(versionObject, "WPE backend"_s, String::fromUTF8(wpe_loader_get_loaded_implementation_library_name()));
    }
#endif

    stopTable();
    jsonObject->setObject("Version Information"_s, WTFMove(versionObject));

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
    addTableRow(displayObject, "Font Scaling DPI"_s, String::number(fontDPI()));
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
    jsonObject->setObject("Display Information"_s, WTFMove(displayObject));

    auto hardwareAccelerationObject = JSON::Object::create();
    startTable("Hardware Acceleration Information"_s);
    addTableRow(hardwareAccelerationObject, "Policy"_s, policy);

#if ENABLE(WEBGL)
    addTableRow(hardwareAccelerationObject, "WebGL enabled"_s, webGLEnabled(request) ? "Yes"_s : "No"_s);
#endif

#if USE(SKIA)
    addTableRow(hardwareAccelerationObject, "2D canvas"_s, canvasAccelerationEnabled(request) ? "Accelerated"_s : "Unaccelerated"_s);
#endif

    if (policy != "never"_s) {
        addTableRow(jsonObject, "API"_s, String::fromUTF8(openGLAPI()));
#if PLATFORM(GTK)
        if (usingDMABufRenderer) {
            addTableRow(hardwareAccelerationObject, "Renderer"_s, dmabufRendererWithSupportedBuffers());
#if USE(LIBDRM)
            addTableRow(hardwareAccelerationObject, "Buffer format"_s, renderBufferDescription(request));
#endif
        }
#elif PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
        if (usingWPEPlatformAPI) {
            addTableRow(hardwareAccelerationObject, "Renderer"_s, dmabufRendererWithSupportedBuffers());
#if USE(LIBDRM)
            addTableRow(hardwareAccelerationObject, "Buffer format"_s, renderBufferDescription(request));
#endif
        }
#endif
        addTableRow(hardwareAccelerationObject, "Native interface"_s, uiProcessContextIsEGL() ? "EGL"_s : "None"_s);

        if (uiProcessContextIsEGL() && eglGetCurrentContext() != EGL_NO_CONTEXT)
            addEGLInfo(hardwareAccelerationObject);
    }

    stopTable();
    jsonObject->setObject("Hardware Acceleration Information"_s, WTFMove(hardwareAccelerationObject));

#if PLATFORM(GTK)
    if (usingDMABufRenderer && policy != "never"_s) {
        std::unique_ptr<PlatformDisplay> platformDisplay;
#if USE(GBM)
        UnixFileDescriptor fd;
        struct gbm_device* device = nullptr;
        const char* disableGBM = getenv("WEBKIT_DMABUF_RENDERER_DISABLE_GBM");
        if (!disableGBM || !strcmp(disableGBM, "0")) {
            auto drmDevice = drmMainDevice();
            if (!drmDevice.isNull()) {
                const auto& filename = !drmDevice.renderNode.isNull() ? drmDevice.renderNode : drmDevice.primaryNode;
                fd = UnixFileDescriptor { open(filename.data(), O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
                if (fd) {
                    device = gbm_create_device(fd.value());
                    if (device)
                        platformDisplay = PlatformDisplayGBM::create(device);
                }
            }
        }
#endif
        if (!platformDisplay)
            platformDisplay = PlatformDisplaySurfaceless::create();

        if (platformDisplay) {
            auto hardwareAccelerationObject = JSON::Object::create();
            startTable("Hardware Acceleration Information (Render Process)"_s);

            addTableRow(hardwareAccelerationObject, "Platform"_s, String::fromUTF8(platformDisplay->type() == PlatformDisplay::Type::Surfaceless ? "Surfaceless"_s : "GBM"_s));

#if USE(GBM)
            if (platformDisplay->type() == PlatformDisplay::Type::GBM) {
                if (drmVersion* version = drmGetVersion(gbm_device_get_fd(device))) {
                    addTableRow(hardwareAccelerationObject, "DRM version"_s, makeString(unsafeSpan(version->name), " ("_s, unsafeSpan(version->desc), ") "_s, version->version_major, '.', version->version_minor, '.', version->version_patchlevel, ". "_s, unsafeSpan(version->date)));
                    drmFreeVersion(version);
                }
            }
#endif

            if (uiProcessContextIsEGL()) {
                GLContext::ScopedGLContext glContext(GLContext::createOffscreen(*platformDisplay));
                addEGLInfo(hardwareAccelerationObject);
            } else {
                // Create the context in a different thread to ensure it doesn't affect any current context in the main thread.
                WorkQueue::create("GPU handler EGL context"_s)->dispatchSync([&] {
                    auto glContext = GLContext::createOffscreen(*platformDisplay);
                    glContext->makeContextCurrent();
                    addEGLInfo(hardwareAccelerationObject);
                });
            }

            stopTable();
            jsonObject->setObject("Hardware Acceleration Information (Render process)"_s, WTFMove(hardwareAccelerationObject));

            // Clear the contexts used by the display before it's destroyed.
            platformDisplay->clearGLContexts();
        }

#if USE(GBM)
        if (device)
            gbm_device_destroy(device);
#endif
    }
#endif

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
    if (usingWPEPlatformAPI) {
        std::unique_ptr<PlatformDisplay> platformDisplay;
#if USE(GBM)
        UnixFileDescriptor fd;
        struct gbm_device* device = nullptr;
        if (auto* drmDevice = wpe_display_get_drm_device(wpe_display_get_primary())) {
            const char* node = wpe_drm_device_get_render_node(drmDevice);
            if (!node)
                node = wpe_drm_device_get_primary_node(drmDevice);
            fd = UnixFileDescriptor { open(node, O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
            if (fd) {
                device = gbm_create_device(fd.value());
                if (device)
                    platformDisplay = PlatformDisplayGBM::create(device);
            }
        }
#endif
        if (!platformDisplay)
            platformDisplay = PlatformDisplaySurfaceless::create();

        if (platformDisplay) {
            auto hardwareAccelerationObject = JSON::Object::create();
            startTable("Hardware Acceleration Information (Render Process)"_s);

            addTableRow(hardwareAccelerationObject, "Platform"_s, String::fromUTF8(platformDisplay->type() == PlatformDisplay::Type::Surfaceless ? "Surfaceless"_s : "GBM"_s));

#if USE(GBM)
            if (platformDisplay->type() == PlatformDisplay::Type::GBM) {
                if (drmVersion* version = drmGetVersion(fd.value())) {
                    addTableRow(hardwareAccelerationObject, "DRM version"_s, makeString(unsafeSpan(version->name), " ("_s, unsafeSpan(version->desc), ") "_s, version->version_major, '.', version->version_minor, '.', version->version_patchlevel, ". "_s, unsafeSpan(version->date)));
                    drmFreeVersion(version);
                }
            }
#endif

            {
                GLContext::ScopedGLContext glContext(GLContext::createOffscreen(*platformDisplay));
                addEGLInfo(hardwareAccelerationObject);
            }

            stopTable();
            jsonObject->setObject("Hardware Acceleration Information (Render process)"_s, WTFMove(hardwareAccelerationObject));

            platformDisplay->clearGLContexts();
        }

#if USE(GBM)
        if (device)
            gbm_device_destroy(device);
#endif
    }
#endif

    auto infoAsString = jsonObject->toJSONString();
    g_string_append_printf(html, "<script>function copyAsJSON() { "
        "var textArea = document.createElement('textarea');"
        "textArea.value = JSON.stringify(%s, null, 4);"
        "document.body.appendChild(textArea);"
        "textArea.focus();"
        "textArea.select();"
        "document.execCommand('copy');"
        "document.body.removeChild(textArea);"
        "}</script></head><body>", infoAsString.utf8().data());
#if PLATFORM(GTK)
    // WPE doesn't seem to pass clipboard data yet.
    g_string_append(html, "<button onclick=\"copyAsJSON()\">Copy to clipboard</button>");
#endif
    g_string_append(html, "<button onclick=\"window.location.href='webkit://gpu/stdout'\">Print in stdout</button>");
    g_string_append_printf(html, "%s</body></html>", tablesBuilder.toString().utf8().data());
    gsize streamLength = html->len;
    GRefPtr<GInputStream> stream = adoptGRef(g_memory_input_stream_new_from_data(g_string_free(html, FALSE), streamLength, g_free));
    webkit_uri_scheme_request_finish(request, stream.get(), streamLength, "text/html");

    if (requestURL.path() == "/stdout"_s)
        WTFLogAlways("GPU information\n%s", prettyPrintJSON(infoAsString).utf8().data());
}

} // namespace WebKit

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
