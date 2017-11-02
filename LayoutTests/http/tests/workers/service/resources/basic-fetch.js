async function test()
{
    try {
        await navigator.serviceWorker.register("resources/basic-fetch-worker.js", { });

        var response = await fetch("test1");
        log("test1 status code: " + response.status);
        log("test1 status text: " + response.statusText);
        log("test1 header Hello: " + response.headers.get("hello"));

        if (window.internals)
            log("test1 source: " + internals.fetchResponseSource(response));

        var response = await fetch("test2");
        log("test2 status code: " + response.status);
        log("test2 status text: " + response.statusText);

        if (window.internals)
            log("test2 source: " + internals.fetchResponseSource(response));

        try {
            response = await fetch("test3");
            log("test3 fetch succeeded unexpectedly");
            log("test3 status code: " + response.status);
            log("test3 status text: " + response.statusText);

        } catch (e) {
            log("test3 fetch failed as expected");
        }
    } catch(e) {
        log("Got exception: " + e);
    }
    finishSWTest();
}

test();
