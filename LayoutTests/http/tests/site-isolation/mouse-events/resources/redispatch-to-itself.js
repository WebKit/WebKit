// When a web process's hit test for a UI-process request reaches a frame hosted by another process, it replies with
// that frame and the point in its coordinates, and the UI process re-sends the request there. A web process controls
// that reply, so the UI process must only follow it to a frame inside the part of this page that the replying process
// hosts. These helpers answer requests with this frame itself, which the UI process should reject.

// The encoded RemoteUserInputEventData naming this frame.
function remoteUserInputEventDataForThisFrame()
{
    return [
        { type: 'FrameID', value: [IPC.frameID[0]] },
        { type: 'double', value: 10 },
        { type: 'double', value: 10 },
    ];
}

// Answers the first few requests with the given message name as if the hit test had found this frame, hosted by
// another process. Stopping after a few keeps a UI process that follows every answer from looping forever. Returns a
// function that reports how many requests arrived.
function forgeRedispatchToItself(messageName, replyArguments)
{
    const requestName = IPC.messages[messageName].name;
    const replyName = IPC.messages[messageName + 'Reply'].name;
    let count = 0;
    IPC.addIncomingMessageListener('UI', message => {
        if (message.name !== requestName || message.listenerID === undefined)
            return;
        if (++count > 3)
            return;
        IPC.sendMessage('UI', message.listenerID, replyName, replyArguments);
    });
    return () => count;
}

function takeInvalidMessageString()
{
    const reply = IPC.sendSyncMessage('UI', 0, IPC.messages.WebProcessProxy_TakeInvalidMessageStringForTesting.name, 1000, []);
    return reply?.arguments?.[0]?.value ?? '';
}

// Resolves with the first invalid message string the UI process records, or with '' after about two seconds.
async function waitForInvalidMessageString()
{
    for (let i = 0; i < 40; ++i) {
        const invalidMessage = takeInvalidMessageString();
        if (invalidMessage)
            return invalidMessage;
        await new Promise(resolve => setTimeout(resolve, 50));
    }
    return '';
}
