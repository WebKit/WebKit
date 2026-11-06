const MockOutcome = { Success: 0, UserCancelled: 1, Failure: 2, NoResponse: 3 };
const messagePrefix = "WebPhoneNumberPossessionCoordinatorProxy_";

let coreIPC = null;
let isRedirecting = false;

function redirectToLocalhost()
{
    if (location.hostname != "127.0.0.1")
        return;
    isRedirecting = true;
    location.hostname = "localhost";
}

function hasPhoneNumberPossessionIPC()
{
    return !!window.IPC && !!IPC.messages[messagePrefix + "RequestPhoneNumberPossession"]?.arguments;
}

async function setUpPhoneNumberPossessionIPC()
{
    if (!coreIPC)
        coreIPC = await import("/ipc/coreipc.js");
}

function messageName(name)
{
    return coreIPC.CoreIPC.messages[messagePrefix + name].name;
}

function serializeArguments(name, args)
{
    return coreIPC.ArgumentSerializer.serializeArguments(coreIPC.CoreIPC.messages[messagePrefix + name].arguments, args);
}

function sendWithReply(name, args)
{
    return new Promise(resolve => {
        IPC.connectionForProcessTarget("UI").sendWithAsyncReply(IPC.pageID, messageName(name), serializeArguments(name, args), resolve);
    });
}

function makeRequest(overrides = { })
{
    return {
        challenge: { span: [1, 2, 3, 4] },
        relyingPartyIdentifier: "localhost",
        desiredResponseType: 0,
        phoneNumberSuffixHint: null,
        pnpiSuffixHint: { },
        knownCarrierPubKey: { },
        ...overrides,
    };
}

async function requestPhoneNumberPossession(frameID, request = makeRequest())
{
    const reply = await sendWithReply("RequestPhoneNumberPossession", { frameID, options: request });
    const [{ hasValue }] = coreIPC.ArgumentParser.parseReply(reply, [{ name: "hasValue", type: "bool" }]);
    if (hasValue)
        return { hasValue };
    const [result] = coreIPC.ArgumentParser.parseReply(reply, [
        { name: "hasValue", type: "bool" },
        { name: "code", type: "uint8_t" },
        { name: "message", type: "String" },
    ]);
    return result;
}

async function isAvailable(frameID)
{
    const reply = await sendWithReply("IsAvailable", { frameID });
    const [{ result }] = coreIPC.ArgumentParser.parseReply(reply, [{ name: "result", type: "bool" }]);
    return result;
}

function cancel(frameID)
{
    return sendWithReply("Cancel", { frameID });
}

function setMockConfiguration(configuration)
{
    IPC.sendMessage("UI", IPC.pageID, messageName("SetMockConfiguration"), serializeArguments("SetMockConfiguration", { configuration }));
}

function takeInvalidMessageString()
{
    return IPC.sendSyncMessage("UI", 0, IPC.messages.WebProcessProxy_TakeInvalidMessageStringForTesting.name, 1000, []).arguments[0].value;
}

function frameIDOf(frameWindow)
{
    return frameWindow.IPC.frameID[0];
}

async function activate()
{
    await UIHelper.activateAt(10, 10);
}

function phoneNumberPossessionIPCTest(test, description)
{
    if (isRedirecting)
        return;
    promise_test(async () => {
        if (!hasPhoneNumberPossessionIPC())
            return;
        await setUpPhoneNumberPossessionIPC();
        await test();
    }, description);
}
