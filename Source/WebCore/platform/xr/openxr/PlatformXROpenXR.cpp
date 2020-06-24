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
#include "PlatformXROpenXR.h"

#if ENABLE(WEBXR)

#include "Logging.h"
#if USE_OPENXR
#include <wtf/Optional.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>
#endif // USE_OPENXR
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

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

#define RETURN_IF_FAILED(result, call, instance)                                                    \
    if (XR_FAILED(result)) {                                                                        \
        LOG(XR, "%s %s: %s\n", __func__, call, resultToString(result, instance).utf8().data()); \
        return;                                                                                     \
    }                                                                                               \

#endif // USE_OPENXR

struct Instance::Impl {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Impl();
    ~Impl();

#if USE_OPENXR
    Optional<Vector<SessionMode>> collectSupportedSessionModes(OpenXRDevice&);
    XrInstance xrInstance() const { return m_instance; }
#endif

private:
#if USE_OPENXR
    void enumerateApiLayerProperties() const;
    void enumerateInstanceExtensionProperties() const;

    XrInstance m_instance { XR_NULL_HANDLE };
#endif // USE_OPENXR
};

void Instance::Impl::enumerateApiLayerProperties() const
{
#if USE_OPENXR
    uint32_t propertyCountOutput { 0 };
    XrResult result = xrEnumerateApiLayerProperties(0, &propertyCountOutput, nullptr);
    RETURN_IF_FAILED(result, "xrEnumerateApiLayerProperties()", m_instance);

    if (!propertyCountOutput) {
        LOG(XR, "xrEnumerateApiLayerProperties(): no properties\n");
        return;
    }

    Vector<XrApiLayerProperties> properties(propertyCountOutput);
    result = xrEnumerateApiLayerProperties(propertyCountOutput, nullptr, properties.data());
    RETURN_IF_FAILED(result, "xrEnumerateApiLayerProperties()", m_instance);
    LOG(XR, "xrEnumerateApiLayerProperties(): %zu properties\n", properties.size());
#endif
}

void Instance::Impl::enumerateInstanceExtensionProperties() const
{
#if USE_OPENXR
    uint32_t propertyCountOutput { 0 };
    XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &propertyCountOutput, nullptr);
    RETURN_IF_FAILED(result, "xrEnumerateInstanceExtensionProperties()", m_instance);
    if (!propertyCountOutput) {
        LOG(XR, "xrEnumerateInstanceExtensionProperties(): no properties\n");
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
    LOG(XR, "xrEnumerateInstanceExtensionProperties(): %zu extension properties\n", properties.size());
    for (auto& property : properties) {
        LOG(XR, "  extension '%s', version %u\n",
            property.extensionName, property.extensionVersion);
    }
#endif
}

Instance::Impl::Impl()
{
    LOG(XR, "OpenXR: initializing\n");

    enumerateApiLayerProperties();
    enumerateInstanceExtensionProperties();

    static const char* s_applicationName = "WebXR (WebKit)";
    static const uint32_t s_applicationVersion = 1;

#if USE_OPENXR
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
    LOG(XR, "xrCreateInstance(): using instance %p\n", m_instance);

#endif // USE_OPENXR
}

Instance::Impl::~Impl()
{
    if (m_instance != XR_NULL_HANDLE)
        xrDestroyInstance(m_instance);
}

#if USE_OPENXR

Optional<Vector<SessionMode>> Instance::Impl::collectSupportedSessionModes(OpenXRDevice& device)
{
    uint32_t viewConfigurationCount;
    auto result = xrEnumerateViewConfigurations(m_instance, device.xrSystemId(), 0, &viewConfigurationCount, nullptr);
    if (result != XR_SUCCESS) {
        LOG(XR, "xrEnumerateViewConfigurations(): error %s\n", resultToString(result, m_instance).utf8().data());
        return WTF::nullopt;
    }

    XrViewConfigurationType viewConfigurations[viewConfigurationCount];
    result = xrEnumerateViewConfigurations(m_instance, device.xrSystemId(), viewConfigurationCount, &viewConfigurationCount, viewConfigurations);
    if (result != XR_SUCCESS) {
        LOG(XR, "xrEnumerateViewConfigurations(): error %s\n", resultToString(result, m_instance).utf8().data());
        return WTF::nullopt;
    }

    Vector<SessionMode> supportedModes;
    for (uint32_t i = 0; i < viewConfigurationCount; ++i) {
        auto viewConfigurationProperties = createStructure<XrViewConfigurationProperties, XR_TYPE_VIEW_CONFIGURATION_PROPERTIES>();
        result = xrGetViewConfigurationProperties(m_instance, device.xrSystemId(), viewConfigurations[i], &viewConfigurationProperties);
        if (result != XR_SUCCESS) {
            LOG(XR, "xrGetViewConfigurationProperties(): error %s\n", resultToString(result, m_instance).utf8().data());
            return WTF::nullopt;
        }
        switch (viewConfigurationProperties.viewConfigurationType) {
            case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
                supportedModes.append(SessionMode::ImmersiveAr);
                break;
            case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
                supportedModes.append(SessionMode::ImmersiveVr);
                break;
            default:
                break;
        };
    }
    return supportedModes;
}

#endif // USE_OPENXR

Instance& Instance::singleton()
{
    static LazyNeverDestroyed<Instance> s_instance;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag,
        [&] {
            s_instance.construct();
        });
    return s_instance.get();
}

Instance::Instance()
    : m_impl(makeUniqueRef<Impl>())
{
}

void Instance::enumerateImmersiveXRDevices()
{
#if USE_OPENXR
    if (m_impl->xrInstance() == XR_NULL_HANDLE) {
        LOG(XR, "%s Unable to enumerate XR devices. No XrInstance present\n", __FUNCTION__);
        return;
    }

    auto systemGetInfo = createStructure<XrSystemGetInfo, XR_TYPE_SYSTEM_GET_INFO>();
    systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XrSystemId systemId;
    XrResult result = xrGetSystem(m_impl->xrInstance(), &systemGetInfo, &systemId);
    if (result != XR_SUCCESS) {
        LOG(XR, "xrGetSystem(): error %s\n", resultToString(result, m_impl->xrInstance()).utf8().data());
        return;
    }

    auto device = makeUnique<OpenXRDevice>();
    device->setXrSystemId(systemId);
    auto sessionModes = m_impl->collectSupportedSessionModes(*device);
    if (sessionModes) {
        for (auto& mode : sessionModes.value()) {
            // TODO: fill in features
            device->setEnabledFeatures(mode, { });
        }
    }

    m_immersiveXRDevices.append(WTFMove(device));
#endif // USE_OPENXR
}

} // namespace PlatformXR

#endif // ENABLE(WEBXR)
