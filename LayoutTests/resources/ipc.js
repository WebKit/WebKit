function randomIPCID() {
    return Math.floor(Math.random() * 10000) + 1;
}

function asyncFlush(processTarget) {
    if (!IPC.processTargets.includes(processTarget))
        throw Error("Invalid processTarget passed to asyncFlush")
    return IPC.sendMessage(processTarget, 0, IPC.messages.IPCTester_AsyncPing.name, [{type: "uint32_t", value: 88}]);
}

function syncFlush(processTarget) {
    if (!IPC.processTargets.includes(processTarget))
        throw Error("Invalid processTarget passed to syncFlush")

    let reply = IPC.sendSyncMessage(processTarget, 0, IPC.messages.IPCTester_SyncPing.name, 1000, [{type: "uint32_t", value: 77}]);
    const firstResult = reply.arguments[0];
    return firstResult.type == "uint32_t" && firstResult.value == 78;
}