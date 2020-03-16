/*
 * Copyright (C) 2020 Igalia, S.L.
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
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "PlatformXR.h"

#if ENABLE(WEBXR)

#if USE_OPENXR
#include <openxr/openxr.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>
#endif // USE_OPENXR
#include <wtf/NeverDestroyed.h>

namespace PlatformXR {

#if USE_OPENXR

template<typename T, XrStructureType StructureType>
T createStructure()
{
    T object;
    std::memset(&object, 0, sizeof(T));
    object.type = StructureType;
    object.next = nullptr;
    return object;
}

String resultToString(XrResult value, XrInstance instance)
{
    char buffer[XR_MAX_RESULT_STRING_SIZE];
    XrResult result = xrResultToString(instance, value, buffer);
    if (result == XR_SUCCESS)
        return String(buffer);
    return makeString("<unknown ", int(value), ">");
}

#define RETURN_IF_FAILED(result, call, instance)                                                     \
    if (XR_FAILED(result)) {                                                                         \
        WTFLogAlways("%s %s: %s\n", __func__, call, resultToString(result, instance).utf8().data()); \
        return;                                                                                      \
    }                                                                                                \

#endif // USE_OPENXR

struct Instance::Impl {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Impl();
    ~Impl();

#if USE_OPENXR
    XrInstance m_instance { XR_NULL_HANDLE };
#endif // USE_OPENXR
};

Instance::Impl::Impl()
{
#if USE_OPENXR
    WTFLogAlways("OpenXR: initializing\n");

    [&] {
        uint32_t propertyCountOutput { 0 };
        XrResult result = xrEnumerateApiLayerProperties(0, &propertyCountOutput, nullptr);
        RETURN_IF_FAILED(result, "xrEnumerateApiLayerProperties()", m_instance);

        if (!propertyCountOutput) {
            WTFLogAlways("xrEnumerateApiLayerProperties(): no properties\n");
            return;
        }

        Vector<XrApiLayerProperties> properties(propertyCountOutput);
        result = xrEnumerateApiLayerProperties(propertyCountOutput, nullptr, properties.data());
        RETURN_IF_FAILED(result, "xrEnumerateApiLayerProperties()", m_instance);

        WTFLogAlways("xrEnumerateApiLayerProperties(): %zu properties\n", properties.size());
    }();

    [&] {
        uint32_t propertyCountOutput { 0 };
        XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &propertyCountOutput, nullptr);
        RETURN_IF_FAILED(result, "xrEnumerateInstanceExtensionProperties()", m_instance);

        if (!propertyCountOutput) {
            WTFLogAlways("xrEnumerateInstanceExtensionProperties(): no properties\n");
            return;
        }

        Vector<XrExtensionProperties> properties(propertyCountOutput,
            [] {
                XrExtensionProperties object;
                std::memset(&object, 0, sizeof(XrExtensionProperties));
                object.type = XR_TYPE_EXTENSION_PROPERTIES;
                return object;
            }());
        uint32_t propertyCountWritten { 0 };
        result = xrEnumerateInstanceExtensionProperties(nullptr, propertyCountOutput, &propertyCountWritten, properties.data());
        RETURN_IF_FAILED(result, "xrEnumerateInstanceExtensionProperties()", m_instance);

        WTFLogAlways("xrEnumerateInstanceExtensionProperties(): %zu extension properties\n", properties.size());
        for (auto& property : properties) {
            WTFLogAlways("  extension '%s', version %u\n",
                property.extensionName, property.extensionVersion);
        }
    }();

    static const char* s_applicationName = "WebXR (WebKit)";
    static const uint32_t s_applicationVersion = 1;

    {
        auto createInfo = createStructure<XrInstanceCreateInfo, XR_TYPE_INSTANCE_CREATE_INFO>();
        createInfo.createFlags = 0;
        std::memcpy(createInfo.applicationInfo.applicationName, s_applicationName, XR_MAX_APPLICATION_NAME_SIZE);
        createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        createInfo.applicationInfo.applicationVersion = s_applicationVersion;
        createInfo.enabledApiLayerCount = 0;
        createInfo.enabledExtensionCount = 0;

        XrInstance instance;
        XrResult result = xrCreateInstance(&createInfo, &instance);
        RETURN_IF_FAILED(result, "xrCreateInstance()", m_instance);

        m_instance = instance;
        WTFLogAlways("xrCreateInstance(): using instance %p\n", m_instance);
    }
#endif // USE_OPENXR
}

Instance::Impl::~Impl()
{
}

Instance& Instance::singleton()
{
    static LazyNeverDestroyed<Instance> s_instance;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag,
        [&] {
            s_instance->m_impl = makeUnique<Impl>();
        });
    return s_instance.get();
}

Instance::Instance() = default;
Instance::~Instance() = default;

} // namespace PlatformXR

#endif // ENABLE(WEBXR)
