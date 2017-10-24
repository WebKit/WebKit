function done()
{
    finishSWTest();
}

async function test()
{
    try {
        await navigator.serviceWorker.register("resources/basic-ServiceWorker-postMessage-worker.js", { });

        navigator.serviceWorker.controller.postMessage({ status: 200, statusText: "Worker received the message event." });

        let response = await fetch("test1");
        if (response.status == 200)
            log("PASS: Status code is " + response.status);
        else
            log("FAIL: Status code is " + response.status);

        if (response.statusText.startsWith("Worker received the message"))
            log("PASS: Status text is " + response.statusText);
        else
            log("FAIL: Status text is " + response.statusText);

        if (window.internals) {
            let source = internals.fetchResponseSource(response);
            if (source === "Service worker")
                log("PASS: Source is " + source);
            else
                log("FAIL: Source is " + source);
        }
    } catch(e) {
        log("Got exception: " + e);
    }
    finishSWTest();
}

test();
