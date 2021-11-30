#pragma once

#include "AbstractWorker.h"
#include "WorkerOptions.h"

namespace WebCore {

class MessagePort;

class ShadowRealm final : public AbstractWorker {
    WTF_MAKE_ISO_ALLOCATED(ShadowRealm);
public:
    static Ref<ShadowRealm> create(String&& scriptURL, std::optional<std::variant<String, WorkerOptions>>&& options) { return adoptRef(*new ShadowRealm(WTFMove(scriptURL), WTFMove(options))); }

    MessagePort* port() const;
private:
    ShadowRealm(String&&, std::optional<std::variant<String, WorkerOptions>>&&);

    EventTargetInterface eventTargetInterface() const final;
    ScriptExecutionContext* scriptExecutionContext() const final;
};

} // namespace WebCore
