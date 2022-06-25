async function waitForOnlineEvent()
{
    var promise = new Promise((resolve, reject) => {
        self.addEventListener("online", () => {
            resolve("online");
        });
        self.addEventListener("offline", () => {
            resolve("offline");
        });
        setTimeout(() => {
            reject("No online event");
        }, 2000);
    });
    return await promise;
}

async function doTest(event)
{
    try {
        if (!event.data.startsWith("ONLINE")) {
            event.source.postMessage("FAIL: received unexpected message from client");
            return;
        }

        if (!self.internals) {
            event.source.postMessage("FAIL: test require internals");
            return;
        }

        internals.setOnline(true);
        var eventName = await waitForOnlineEvent();
        if (!navigator.onLine)
            event.source.postMessage("FAIL: test 1");
        if (eventName !== "online")
            event.source.postMessage("FAIL: test 2, got " + eventName);

        internals.setOnline(false);
        var eventName = await waitForOnlineEvent();
        if (navigator.onLine)
            event.source.postMessage("FAIL: test 3");
        if (eventName !== "offline")
            event.source.postMessage("FAIL: test 4, got " + eventName);

        event.source.postMessage("PASS");
    } catch (e) {
        event.source.postMessage(e);
    }
}

self.addEventListener("message", doTest);
