// When a web process's hit test for a tap reaches a frame hosted by another process, it replies with that frame and the
// point in its coordinates, and the UI process re-sends the request there. A web process controls that reply, so the UI
// process must only follow it to a frame inside the part of this page that the replying process hosts. These helpers
// answer a request with a frame of the test's choosing, which the UI process should reject.

// The reply arguments for each request, given the encoded RemoteUserInputEventData naming the forged target.
const redispatchReplyArguments = {
    WebPage_PotentialTapAtPosition: remote => [{ type: 'bool', value: 1 }, ...remote],
    WebPage_CommitPotentialTap: remote => [{ type: 'bool', value: 1 }, remote[0]],
    WebPage_HandleDoubleTapForDoubleClickAtPoint: remote => [{ type: 'bool', value: 1 }, ...remote],
    WebPage_HandleTwoFingerTapAtPoint: remote => [{ type: 'bool', value: 1 }, ...remote],
    WebPage_TapHighlightAtPosition: remote => [{ type: 'bool', value: 1 }, ...remote],
    WebPage_AttemptSyntheticClick: remote => [{ type: 'bool', value: 1 }, ...remote],
};

function thisFrameIdentifier()
{
    return IPC.frameID[0].toString();
}

// Counts the requests with the given message name that this frame's process receives. If a target frame is given,
// answers the first few as if the point were over that frame, hosted by another process. Stopping after a few keeps a UI
// process that follows every answer from looping forever. Returns a function that reports the count.
function countRequestsAndForgeRedispatch(messageName, forgedTargetFrameID)
{
    const requestName = IPC.messages[messageName].name;
    const replyName = IPC.messages[messageName + 'Reply'].name;
    const remote = forgedTargetFrameID ? [
        { type: 'FrameID', value: [BigInt(forgedTargetFrameID)] },
        { type: 'double', value: 10 },
        { type: 'double', value: 10 },
    ] : null;
    let count = 0;
    IPC.addIncomingMessageListener('UI', message => {
        if (message.name !== requestName)
            return;
        if (++count > 3 || !remote || message.listenerID === undefined)
            return;
        IPC.sendMessage('UI', message.listenerID, replyName, redispatchReplyArguments[messageName](remote));
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

// Asks a frame running redispatch-request-counter.html how many requests its process received.
function requestCountIn(target)
{
    return new Promise(resolve => {
        addEventListener('message', function listener(event) {
            if (event.data?.type !== 'request-count')
                return;
            removeEventListener('message', listener);
            resolve(event.data.count);
        });
        target.postMessage({ type: 'get-request-count' }, '*');
    });
}

// Waits for a frame running redispatch-request-counter.html to report its frame identifier.
function counterFrameIdentifier()
{
    return new Promise(resolve => {
        addEventListener('message', function listener(event) {
            if (event.data?.type !== 'counter-ready')
                return;
            removeEventListener('message', listener);
            resolve(event.data.frameID);
        });
    });
}

// Sends the request with `gesture`, answering it with `forgedTargetFrameID`, and checks that the UI process rejected the
// answer: it sent no further request to this process and, unless `expectFlagged` is false, flagged it as invalid.
// `otherProcessRequestCount`, if given, must report that the forged target's process got no request either. Resolves with
// the result line.
async function expectRedispatchIsRejected({ messageName, forgedTargetFrameID, gesture, otherProcessRequestCount, expectFlagged = true })
{
    const requestCount = countRequestsAndForgeRedispatch(messageName, forgedTargetFrameID ?? thisFrameIdentifier());
    await gesture();

    const invalidMessage = await waitForInvalidMessageString();
    // Give a redispatch that was wrongly followed time to arrive.
    await new Promise(resolve => setTimeout(resolve, 200));
    if (!requestCount())
        return `FAIL: the gesture sent no ${messageName} to this process.`;
    if (requestCount() != 1)
        return `FAIL: the UI process followed the redispatch; this process got ${requestCount()} requests.`;
    if (otherProcessRequestCount) {
        const otherCount = await otherProcessRequestCount();
        if (otherCount)
            return `FAIL: the UI process followed the redispatch; the forged target's process got ${otherCount} requests.`;
    }
    if (expectFlagged && !invalidMessage.includes('Message check failed'))
        return `FAIL: the UI process didn't flag the redispatch. Invalid message: '${invalidMessage}'`;
    return 'PASS';
}

function finishRedispatchTest(summary, result)
{
    document.getElementById('result').textContent = summary + '\n' + result;
    if (window.testRunner)
        testRunner.notifyDone();
}
