#include "config.h"
#include "InspectorAPI.h"
#include "API/JSContextRefInternal.h"
#include "APICast.h"
#include <inspector/InspectorFrontendChannel.h>
#include <memory>

#include "JSAPIGlobalObject.h"
#include "JSGlobalObject.h"
#include "JSCInlines.h"
#include "JSLock.h"
#include "JSRemoteInspector.h"
#include "JSRemoteInspectorServer.h"

#include <cstdio>
#include <inspector/JSGlobalObjectInspectorController.h>
#include <inspector/remote/RemoteInspector.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

class RustFrontendChannel final : public FrontendChannel {
public:
    explicit RustFrontendChannel(JSC::JSAPIGlobalObject& global)
        : m_global(global)
    {
    }
    virtual ~RustFrontendChannel() = default;

private:
    FrontendChannel::ConnectionType connectionType() const override { return FrontendChannel::ConnectionType::Remote; }
    void sendMessageToFrontend(const WTF::String& message) override
    {
        if (auto callback = m_global.inspectorCallback())
            callback(message.utf8().data());
    }
    JSC::JSAPIGlobalObject& m_global;
};

} // namespace Inspector

extern "C" {

void JSInspectorSetCallback(JSGlobalContextRef context, InspectorMessageCallback callback)
{
    if (!context)
        return;

    JSC::JSGlobalObject* globalObject = toJS(context);
    if (!globalObject)
        return;

    auto& inspectorController = globalObject->inspectorController();

    JSC::JSAPIGlobalObject* apiGlobal = jsCast<JSC::JSAPIGlobalObject*>(globalObject);
    if (!apiGlobal)
        return;

    apiGlobal->setInspectorCallback(callback);

    // Disconnect existing frontend if present
    if (auto* existingChannel = apiGlobal->frontendChannel()) {
        inspectorController.disconnectFrontend(*existingChannel);
        apiGlobal->clearFrontendChannel();
    }

    if (callback) {
        auto channel = std::make_unique<Inspector::RustFrontendChannel>(*apiGlobal);
        inspectorController.connectFrontend(*channel, false, false);
        apiGlobal->setFrontendChannel(std::move(channel));
    }

    JSGlobalContextSetInspectable(context, callback != nullptr);

    // Note: We intentionally don't call JSRemoteInspectorStart() here because
    // it starts a global remote inspector server that is not needed for direct frontend channel communication.
}

void JSInspectorSendMessage(JSGlobalContextRef context, const char* message)
{
    if (!context || !message)
        return;

    JSC::JSGlobalObject* globalObject = toJS(context);
    if (!globalObject)
        return;

    // IMPORTANT: JSLockHolder is required before calling dispatchMessageFromFrontend.
    // This ensures proper thread safety and VM state management.
    // Without this lock, debugger commands like Debugger.enable can cause hangs
    // because the VM state is not properly managed during heap iteration and
    // code recompilation that occurs when the debugger attaches.
    // See JSGlobalObjectDebuggable::dispatchMessageFromRemote for reference.
    JSC::JSLockHolder locker(&globalObject->vm());

    auto& inspectorController = globalObject->inspectorController();
    inspectorController.dispatchMessageFromFrontend(WTF::String::fromUTF8(message));
}

void JSInspectorDisconnect(JSGlobalContextRef context)
{
    if (!context)
        return;

    JSC::JSGlobalObject* globalObject = toJS(context);
    if (!globalObject)
        return;

    JSC::JSAPIGlobalObject* apiGlobal = jsCast<JSC::JSAPIGlobalObject*>(globalObject);
    if (!apiGlobal)
        return;

    if (auto* channel = apiGlobal->frontendChannel()) {
        auto& inspectorController = globalObject->inspectorController();
        inspectorController.disconnectFrontend(*channel);
        apiGlobal->clearFrontendChannel();
    }

    // Clear the message callback
    apiGlobal->setInspectorCallback(nullptr);
    apiGlobal->setPauseEventCallback(nullptr);

    // Mark as not inspectable
    JSGlobalContextSetInspectable(context, false);
}

bool JSInspectorIsConnected(JSGlobalContextRef context)
{
    if (!context)
        return false;

    JSC::JSGlobalObject* globalObject = toJS(context);
    if (!globalObject)
        return false;

    JSC::JSAPIGlobalObject* apiGlobal = jsCast<JSC::JSAPIGlobalObject*>(globalObject);
    if (!apiGlobal)
        return false;

    return apiGlobal->frontendChannel() != nullptr;
}

void JSInspectorSetPauseEventCallback(
    JSGlobalContextRef context,
    InspectorPauseEventCallback callback
) {
    if (!context)
        return;

    JSC::JSGlobalObject* globalObject = toJS(context);
    if (!globalObject)
        return;

    JSC::JSAPIGlobalObject* apiGlobal = jsCast<JSC::JSAPIGlobalObject*>(globalObject);
    if (!apiGlobal)
        return;

    apiGlobal->setPauseEventCallback(callback);
}

} // extern "C"
