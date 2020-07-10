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
    XrInstance xrInstance() const { return m_instance; }
#endif

private:
#if USE_OPENXR
    void enumerateApiLayerProperties() const;
    void enumerateInstanceExtensionProperties() const;

    XrInstance m_instance { XR_NULL_HANDLE };
#endif // USE_OPENXR
};

#if USE_OPENXR
void Instance::Impl::enumerateApiLayerProperties() const
{
    uint32_t propertyCountOutput { 0 };
    XrResult result = xrEnumerateApiLayerProperties(0, &propertyCountOutput, nullptr);
    RETURN_IF_FAILED(result, "xrEnumerateApiLayerProperties()", m_instance);

    if (!propertyCountOutput) {
        LOG(XR, "xrEnumerateApiLayerProperties(): no properties\n");
        return;
    }

    Vector<XrApiLayerProperties> properties(propertyCountOutput,
        [] {
            XrApiLayerProperties object;
            std::memset(&object, 0, sizeof(XrApiLayerProperties));
            object.type = XR_TYPE_API_LAYER_PROPERTIES;
            return object;
        }());
    result = xrEnumerateApiLayerProperties(propertyCountOutput, &propertyCountOutput, properties.data());

    RETURN_IF_FAILED(result, "xrEnumerateApiLayerProperties()", m_instance);
    LOG(XR, "xrEnumerateApiLayerProperties(): %zu properties\n", properties.size());
}

void Instance::Impl::enumerateInstanceExtensionProperties() const
{
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
}
#endif // USE_OPENXR

Instance::Impl::Impl()
{
#if USE_OPENXR
    LOG(XR, "OpenXR: initializing\n");

    enumerateApiLayerProperties();
    enumerateInstanceExtensionProperties();

    static const char* s_applicationName = "WebXR (WebKit)";
    static const uint32_t s_applicationVersion = 1;

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
#if USE_OPENXR
    if (m_instance != XR_NULL_HANDLE)
        xrDestroyInstance(m_instance);
#endif
}

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
    RETURN_IF_FAILED(result, "xrGetSystem", m_impl->xrInstance());

    m_immersiveXRDevices.append(makeUnique<OpenXRDevice>(systemId, m_impl->xrInstance()));
#endif // USE_OPENXR
}

#if USE_OPENXR
OpenXRDevice::OpenXRDevice(XrSystemId id, XrInstance instance)
    : m_systemId(id)
    , m_instance(instance)
{
    auto systemProperties = createStructure<XrSystemProperties, XR_TYPE_SYSTEM_PROPERTIES>();
    XrResult result = xrGetSystemProperties(instance, m_systemId, &systemProperties);
    if (result == XR_SUCCESS)
        m_supportsOrientationTracking = systemProperties.trackingProperties.orientationTracking == XR_TRUE;
    else
        LOG(XR, "xrGetSystemProperties(): error %s\n", resultToString(result, m_instance).utf8().data());

    collectSupportedSessionModes();
    enumerateConfigurationViews();
}

void OpenXRDevice::collectSupportedSessionModes()
{
    uint32_t viewConfigurationCount;
    auto result = xrEnumerateViewConfigurations(m_instance, m_systemId, 0, &viewConfigurationCount, nullptr);
    RETURN_IF_FAILED(result, "xrEnumerateViewConfigurations", m_instance);

    XrViewConfigurationType viewConfigurations[viewConfigurationCount];
    result = xrEnumerateViewConfigurations(m_instance, m_systemId, viewConfigurationCount, &viewConfigurationCount, viewConfigurations);
    RETURN_IF_FAILED(result, "xrEnumerateViewConfigurations", m_instance);

    for (uint32_t i = 0; i < viewConfigurationCount; ++i) {
        auto viewConfigurationProperties = createStructure<XrViewConfigurationProperties, XR_TYPE_VIEW_CONFIGURATION_PROPERTIES>();
        result = xrGetViewConfigurationProperties(m_instance, m_systemId, viewConfigurations[i], &viewConfigurationProperties);
        if (result != XR_SUCCESS) {
            LOG(XR, "xrGetViewConfigurationProperties(): error %s\n", resultToString(result, m_instance).utf8().data());
            continue;
        }
        auto configType = viewConfigurationProperties.viewConfigurationType;
        switch (configType) {
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
            setEnabledFeatures(SessionMode::Inline, { });
            break;
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
            setEnabledFeatures(SessionMode::ImmersiveVr, { });
            break;
        default:
            continue;
        };
        m_viewConfigurationProperties.add(configType, WTFMove(viewConfigurationProperties));
    }
}

void OpenXRDevice::enumerateConfigurationViews()
{
    for (auto& config : m_viewConfigurationProperties.values()) {
        uint32_t viewCount;
        auto configType = config.viewConfigurationType;
        auto result = xrEnumerateViewConfigurationViews(m_instance, m_systemId, configType, 0, &viewCount, nullptr);
        if (result != XR_SUCCESS) {
            LOG(XR, "%s %s: %s\n", __func__, "xrEnumerateViewConfigurationViews", resultToString(result, m_instance).utf8().data());
            continue;
        }

        Vector<XrViewConfigurationView> configViews(viewCount,
            [] {
                XrViewConfigurationView object;
                std::memset(&object, 0, sizeof(XrViewConfigurationView));
                object.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
                return object;
            }());
        result = xrEnumerateViewConfigurationViews(m_instance, m_systemId, configType, viewCount, &viewCount, configViews.data());
        if (result != XR_SUCCESS)
            continue;
        m_configurationViews.add(configType, WTFMove(configViews));
    }
}

WebCore::IntSize OpenXRDevice::recommendedResolution(SessionMode mode)
{
    auto configType = mode == SessionMode::Inline ? XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO : XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    auto viewsIterator = m_configurationViews.find(configType);
    if (viewsIterator != m_configurationViews.end())
        return { static_cast<int>(viewsIterator->value[0].recommendedImageRectWidth), static_cast<int>(viewsIterator->value[0].recommendedImageRectHeight) };
    return Device::recommendedResolution(mode);
}


#endif // USE_OPENXR

} // namespace PlatformXR

#endif // ENABLE(WEBXR)
