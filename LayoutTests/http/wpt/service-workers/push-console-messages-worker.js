async function doPush(event)
{
    const data = event.data.text();
    self.internals.logReportedConsoleMessage("doPush: " + data);

    const registration = self.registration;

    try {
        if (data === "NO-SHOWNOTIFICATION") {
            if (self.testCallback)
                self.testCallback("PASS");
            return;
        }
        if (data === "SHOWNOTIFICATION-ASYNC") {
            await new Promise(resolve => setTimeout(resolve, 10));
            registration.showNotification(data);
            if (self.testCallback)
                self.testCallback("PASS");
            return;
        }
        if (data === "SHOWNOTIFICATION-SYNC") {
            registration.showNotification(data);
            if (self.testCallback)
                self.testCallback("PASS");
            return;
        }
        if (data === "SHOWNOTIFICATION-LATE") {
            setTimeout(() => {
                try {
                    registration.showNotification(data);
                    if (self.testCallback)
                        self.testCallback("PASS");
                } catch (e) {
                    if (self.testCallback)
                        self.testCallback("FAIL: " + e);
                }
            }, 0);
            return;
        }
        if (self.testCallback)
            self.testCallback("FAIL");
    } catch (e) {
        if (self.testCallback)
            self.testCallback("FAIL: " + e);
    }
}

async function doTest(event)
{
    if (event.data.type !== "PUSHEVENT") {
        event.source.postMessage("FAIL: received unexpected message from client");
        return;
    }

    if (!self.internals) {
        event.source.postMessage("FAIL: need internals");
        return;
    }
    self.internals.enableConsoleMessageReporting();
    self.internals.logReportedConsoleMessage("doTest: " + JSON.stringify(event.data));

    const promise = new Promise(resolve => self.testCallback = resolve);
    promise.then(value => event.source.postMessage(value));
    event.waitUntil(promise);

    event.waitUntil(self.internals.schedulePushEvent(event.data.message));
}

self.addEventListener("message", e => e.waitUntil(doTest(e)));
self.addEventListener("push", e => e.waitUntil(doPush(e)));
