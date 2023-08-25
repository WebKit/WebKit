function asyncFlush(processTarget) {
    if (!IPC.processTargets.includes(processTarget))
        throw Error("Invalid processTarget passed to asyncFlush")
    return IPC.sendMessage(processTarget, 0, IPC.messages.IPCTester_AsyncPing.name, [])
}

function syncFlush(processTarget) {
    if (!IPC.processTargets.includes(processTarget))
        throw Error("Invalid processTarget passed to syncFlush")
    return new Promise((resolve) => {
        IPC.sendSyncMessage(processTarget, 0, IPC.messages.IPCTester_SyncPing.name, 1000, []);
        resolve();
    })
}