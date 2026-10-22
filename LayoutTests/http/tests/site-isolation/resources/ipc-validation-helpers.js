const crossSiteOrigin = "http://localhost:8000";

function waitForMessage(source, matches)
{
    return new Promise((resolve) => {
        function listener(event)
        {
            if ((source && event.source !== source) || !matches(event.data))
                return;
            removeEventListener("message", listener);
            resolve(event.data);
        }
        addEventListener("message", listener);
    });
}

function frameIDArgument(frameID)
{
    return { type: "FrameID", value: [BigInt(frameID)] };
}

async function addSameSiteFrame(t)
{
    const frame = document.createElement("iframe");
    t.add_cleanup(() => frame.remove());
    const loaded = new Promise((resolve) => frame.onload = resolve);
    frame.src = "resources/frame-with-text.html";
    document.body.appendChild(frame);
    await loaded;
    return frame;
}

// The cross-site iframe sends whichever IPC messages it is asked to send.
async function addMessageSenderFrame(t)
{
    const frame = document.createElement("iframe");
    t.add_cleanup(() => frame.remove());
    frame.src = crossSiteOrigin + "/site-isolation/resources/ipc-message-sender-iframe.html";
    document.body.appendChild(frame);
    frame.frameID = (await waitForMessage(frame.contentWindow, (data) => data?.loaded !== undefined)).loaded;
    return frame;
}

async function sendMessageFromFrame(frame, name, args)
{
    const sent = waitForMessage(frame.contentWindow, (data) => data == "sent");
    frame.contentWindow.postMessage({ name, args }, "*");
    await sent;
}

function takeInvalidMessageString()
{
    const reply = IPC.sendSyncMessage("UI", 0, IPC.messages.WebProcessProxy_TakeInvalidMessageStringForTesting.name, 1000, []);
    return reply?.arguments?.[0]?.value ?? "";
}

// Sends the message for a frame that belongs to a popup, which is another page, and returns whether the page check rejected it.
async function isFrameOfAnotherPageRejected(t, name, extraArgs)
{
    const popup = window.open("resources/frame-of-another-page-popup.html");
    t.add_cleanup(() => popup.close());
    await waitForMessage(popup, (data) => data == "opened window loaded");
    takeInvalidMessageString();
    IPC.sendMessage("UI", IPC.pageID, IPC.messages[name].name, [frameIDArgument(popup.IPC.frameID[0]), ...extraArgs]);
    return takeInvalidMessageString().includes("frame->page() == this");
}
