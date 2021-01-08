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

#if ENABLE(WEBXR) && USE(OPENXR)

#include "Logging.h"
#include <openxr/openxr_platform.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Optional.h>
#include <wtf/Scope.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace PlatformXR {

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

#define RETURN_IF_FAILED(result, call, instance, ...)                                           \
    if (XR_FAILED(result)) {                                                                    \
        LOG(XR, "%s %s: %s\n", __func__, call, resultToString(result, instance).utf8().data()); \
        return __VA_ARGS__;                                                                     \
    }

struct Instance::Impl {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    Impl();
    ~Impl();

    XrInstance xrInstance() const { return m_instance; }
    WorkQueue& queue() const { return m_workQueue; }

private:
    void enumerateApiLayerProperties() const;
    bool checkInstanceExtensionProperties() const;

    XrInstance m_instance { XR_NULL_HANDLE };
    Ref<WorkQueue> m_workQueue;
};

void Instance::Impl::enumerateApiLayerProperties() const
{
    ASSERT(&RunLoop::current() == &m_workQueue->runLoop());
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

static bool isExtensionSupported(const char* extensionName, Vector<XrExtensionProperties>& instanceExtensionProperties)
{
    auto position = instanceExtensionProperties.findMatching([extensionName](auto& property) {
        return !strcmp(property.extensionName, extensionName);
    });
    return position != notFound;
}

bool Instance::Impl::checkInstanceExtensionProperties() const
{
    ASSERT(&RunLoop::current() == &m_workQueue->runLoop());
    uint32_t propertyCountOutput { 0 };
    XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &propertyCountOutput, nullptr);
    RETURN_IF_FAILED(result, "xrEnumerateInstanceExtensionProperties", m_instance, false);

    if (!propertyCountOutput) {
        LOG(XR, "xrEnumerateInstanceExtensionProperties(): no properties\n");
        return false;
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
    RETURN_IF_FAILED(result, "xrEnumerateInstanceExtensionProperties", m_instance, false);
#if !LOG_DISABLED
    LOG(XR, "xrEnumerateInstanceExtensionProperties(): %zu extension properties\n", properties.size());
    for (auto& property : properties)
        LOG(XR, "  extension '%s', version %u\n", property.extensionName, property.extensionVersion);
#endif
    if (!isExtensionSupported(XR_MND_HEADLESS_EXTENSION_NAME, properties)) {
        LOG(XR, "Required extension %s not supported", XR_MND_HEADLESS_EXTENSION_NAME);
        return false;
    }

    return true;
}

Instance::Impl::Impl()
    : m_workQueue(WorkQueue::create("OpenXR queue"))
{
    m_workQueue->dispatch([this]() {
        LOG(XR, "OpenXR: initializing\n");

        enumerateApiLayerProperties();

        if (!checkInstanceExtensionProperties())
            return;

        static const char* s_applicationName = "WebXR (WebKit)";
        static const uint32_t s_applicationVersion = 1;

        const char* const enabledExtensions[] = {
            XR_MND_HEADLESS_EXTENSION_NAME,
        };

        auto createInfo = createStructure<XrInstanceCreateInfo, XR_TYPE_INSTANCE_CREATE_INFO>();
        createInfo.createFlags = 0;
        std::memcpy(createInfo.applicationInfo.applicationName, s_applicationName, XR_MAX_APPLICATION_NAME_SIZE);
        createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        createInfo.applicationInfo.applicationVersion = s_applicationVersion;
        createInfo.enabledApiLayerCount = 0;
        createInfo.enabledExtensionCount = WTF_ARRAY_LENGTH(enabledExtensions);
        createInfo.enabledExtensionNames = enabledExtensions;

        XrInstance instance;
        XrResult result = xrCreateInstance(&createInfo, &instance);
        RETURN_IF_FAILED(result, "xrCreateInstance()", m_instance);
        m_instance = instance;
        LOG(XR, "xrCreateInstance(): using instance %p\n", m_instance);
    });
}

Instance::Impl::~Impl()
{
    m_workQueue->dispatch([this] {
        if (m_instance != XR_NULL_HANDLE)
            xrDestroyInstance(m_instance);
    });
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

void Instance::enumerateImmersiveXRDevices(CompletionHandler<void(const DeviceList& devices)>&& callback)
{
    m_impl->queue().dispatch([this, callback = WTFMove(callback)]() mutable {
        auto callbackOnExit = makeScopeExit([&]() {
            callOnMainThread([callback = WTFMove(callback)]() mutable {
                callback({ });
            });
        });

        if (m_impl->xrInstance() == XR_NULL_HANDLE) {
            LOG(XR, "%s Unable to enumerate XR devices. No XrInstance present\n", __FUNCTION__);
            return;
        }

        auto systemGetInfo = createStructure<XrSystemGetInfo, XR_TYPE_SYSTEM_GET_INFO>();
        systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

        XrSystemId systemId;
        XrResult result = xrGetSystem(m_impl->xrInstance(), &systemGetInfo, &systemId);
        RETURN_IF_FAILED(result, "xrGetSystem", m_impl->xrInstance());

        callbackOnExit.release();

        callOnMainThread([this, callback = WTFMove(callback), systemId]() mutable {
            m_immersiveXRDevices = DeviceList::from(makeUniqueRef<OpenXRDevice>(systemId, m_impl->xrInstance(), m_impl->queue(), [this, callback = WTFMove(callback)]() mutable {
                ASSERT(isMainThread());
                callback(m_immersiveXRDevices);
            }));
        });
    });
}

OpenXRDevice::OpenXRDevice(XrSystemId id, XrInstance instance, WorkQueue& queue, CompletionHandler<void()>&& callback)
    : m_systemId(id)
    , m_instance(instance)
    , m_queue(queue)
{
    ASSERT(isMainThread());
    m_queue.dispatch([this, callback = WTFMove(callback)]() mutable {
        auto systemProperties = createStructure<XrSystemProperties, XR_TYPE_SYSTEM_PROPERTIES>();
        auto result = xrGetSystemProperties(m_instance, m_systemId, &systemProperties);
        if (XR_SUCCEEDED(result))
            m_supportsOrientationTracking = systemProperties.trackingProperties.orientationTracking == XR_TRUE;
#if !LOG_DISABLED
        else
            LOG(XR, "xrGetSystemProperties(): error %s\n", resultToString(result, m_instance).utf8().data());
        LOG(XR, "Found XRSystem %lu: \"%s\", vendor ID %d\n", systemProperties.systemId, systemProperties.systemName, systemProperties.vendorId);
#endif

        collectSupportedSessionModes();
        collectConfigurationViews();

        callOnMainThread(WTFMove(callback));
    });
}

OpenXRDevice::~OpenXRDevice()
{
    shutDownTrackingAndRendering();
}

Device::ListOfEnabledFeatures OpenXRDevice::enumerateReferenceSpaces(XrSession& session) const
{
    uint32_t referenceSpacesCount;
    auto result = xrEnumerateReferenceSpaces(session, 0, &referenceSpacesCount, nullptr);
    RETURN_IF_FAILED(result, "xrEnumerateReferenceSpaces", m_instance, { });

    Vector<XrReferenceSpaceType> referenceSpaces(referenceSpacesCount);
    referenceSpaces.fill(XR_REFERENCE_SPACE_TYPE_VIEW, referenceSpacesCount);
    result = xrEnumerateReferenceSpaces(session, referenceSpacesCount, &referenceSpacesCount, referenceSpaces.data());
    RETURN_IF_FAILED(result, "xrEnumerateReferenceSpaces", m_instance, { });

    ListOfEnabledFeatures enabledFeatures;
    for (auto& referenceSpace : referenceSpaces) {
        switch (referenceSpace) {
        case XR_REFERENCE_SPACE_TYPE_VIEW:
            enabledFeatures.append(ReferenceSpaceType::Viewer);
            LOG(XR, "\tDevice supports VIEW reference space");
            break;
        case XR_REFERENCE_SPACE_TYPE_LOCAL:
            enabledFeatures.append(ReferenceSpaceType::Local);
            LOG(XR, "\tDevice supports LOCAL reference space");
            break;
        case XR_REFERENCE_SPACE_TYPE_STAGE:
            enabledFeatures.append(ReferenceSpaceType::LocalFloor);
            enabledFeatures.append(ReferenceSpaceType::BoundedFloor);
            LOG(XR, "\tDevice supports STAGE reference space");
            break;
        case XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT:
            enabledFeatures.append(ReferenceSpaceType::Unbounded);
            LOG(XR, "\tDevice supports UNBOUNDED reference space");
            break;
        default:
            continue;
        }
    }

    return enabledFeatures;
}

void OpenXRDevice::collectSupportedSessionModes()
{
    ASSERT(&RunLoop::current() == &m_queue.runLoop());
    uint32_t viewConfigurationCount;
    auto result = xrEnumerateViewConfigurations(m_instance, m_systemId, 0, &viewConfigurationCount, nullptr);
    RETURN_IF_FAILED(result, "xrEnumerateViewConfigurations", m_instance);

    XrViewConfigurationType viewConfigurations[viewConfigurationCount];
    result = xrEnumerateViewConfigurations(m_instance, m_systemId, viewConfigurationCount, &viewConfigurationCount, viewConfigurations);
    RETURN_IF_FAILED(result, "xrEnumerateViewConfigurations", m_instance);

    // Retrieving the supported reference spaces requires an initialized session. There is no need to initialize all the graphics
    // stuff so we'll use a headless session that will be discarded after getting the info we need.
    auto sessionCreateInfo = createStructure<XrSessionCreateInfo, XR_TYPE_SESSION_CREATE_INFO>();
    sessionCreateInfo.systemId = m_systemId;
    XrSession ephemeralSession;
    result = xrCreateSession(m_instance, &sessionCreateInfo, &ephemeralSession);
    RETURN_IF_FAILED(result, "xrCreateSession", m_instance);

    ListOfEnabledFeatures features = enumerateReferenceSpaces(ephemeralSession);
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
            setEnabledFeatures(SessionMode::Inline, features);
            break;
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
            setEnabledFeatures(SessionMode::ImmersiveVr, features);
            break;
        default:
            continue;
        };
        m_viewConfigurationProperties.add(configType, WTFMove(viewConfigurationProperties));
    }
    xrDestroySession(ephemeralSession);
}

void OpenXRDevice::collectConfigurationViews()
{
    ASSERT(&RunLoop::current() == &m_queue.runLoop());
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

XrViewConfigurationType toXrViewConfigurationType(SessionMode mode)
{
    switch (mode) {
    case SessionMode::ImmersiveVr:
        return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    case SessionMode::Inline:
    case SessionMode::ImmersiveAr:
        return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
    };
    ASSERT_NOT_REACHED();
    return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
}

void OpenXRDevice::initializeTrackingAndRendering(SessionMode mode)
{
    m_queue.dispatch([this, mode]() {
        ASSERT(m_instance != XR_NULL_HANDLE);
        ASSERT(m_session == XR_NULL_HANDLE);

        m_currentViewConfigurationType = toXrViewConfigurationType(mode);
        ASSERT(m_configurationViews.contains(m_currentViewConfigurationType));

        // Create the session.
        auto sessionCreateInfo = createStructure<XrSessionCreateInfo, XR_TYPE_SESSION_CREATE_INFO>();
        sessionCreateInfo.systemId = m_systemId;
        auto result = xrCreateSession(m_instance, &sessionCreateInfo, &m_session);
        RETURN_IF_FAILED(result, "xrEnumerateInstanceExtensionProperties", m_instance);
    });
}

void OpenXRDevice::resetSession()
{
    m_queue.dispatch([this]() {
        if (m_session == XR_NULL_HANDLE)
            return;
        xrDestroySession(m_session);
        m_session = XR_NULL_HANDLE;
    });
}

void OpenXRDevice::shutDownTrackingAndRendering()
{
    resetSession();
}

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(OPENXR)
