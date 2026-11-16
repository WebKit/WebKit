function randomIPCID() {
    return Math.floor(Math.random() * 10000) + 1;
}

function sendWithPromisedReplyForConnection(connection, destinationID, messageName, args) {
    return new Promise((resolve, reject) => {
        function cb(result) {
            if (typeof result == "object" && typeof result.arguments === "object")
                resolve(result);
            else
                reject();
        }
        connection.sendWithAsyncReply(destinationID, messageName, args, cb);
    });
}

if (window.IPC) {
    IPC.sendWithPromisedReply = function(processTarget, destinationID, messageName, args) {
        return sendWithPromisedReplyForConnection(IPC.connectionForProcessTarget(processTarget), destinationID, messageName, args);
    }
}

async function asyncFlush(processTarget) {
    let result = await IPC.sendWithPromisedReply(processTarget, 0, IPC.messages.IPCTester_AsyncPing.name, [{type: "uint32_t", value: 88}])
    if (result.arguments[0].type != "uint32_t" || result.arguments[0].value != 89)
        throw Error("invalid result");
}

function syncFlush(processTarget) {
    if (!IPC.processTargets.includes(processTarget))
        throw Error("Invalid processTarget passed to syncFlush")

    let reply = IPC.sendSyncMessage(processTarget, 0, IPC.messages.IPCTester_SyncPing.name, 1000, [{type: "uint32_t", value: 77}]);
    const firstResult = reply.arguments[0];
    return firstResult.type == "uint32_t" && firstResult.value == 78;
}