async function test()
{
    try {
        var frame = await interceptedFrame("resources/basic-fetch-worker.js", "/");
        var fetch = frame.contentWindow.fetch;

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
            log("FAIL: test3 fetch succeeded unexpectedly");
            log("test3 status code: " + response.status);
            log("test3 status text: " + response.statusText);

        } catch (e) {
            log("PASS: test3 fetch failed as expected");
        }

        var response = await fetch("test4");
        log("test4 status code: " + response.status);

        try {
            response = await fetch("test5");
            log("FAIL: test5 fetch succeeded unexpectedly");
            log("test5 status code: " + response.status);
        } catch (e) {
            log("PASS: test5 fetch failed as expected");
        }

        try {
            response = await fetch("/");
            log("FAIL: / fetch succeeded unexpectedly");
            log("/ status code: " + response.status);
        } catch (e) {
            log("PASS: / fetch failed as expected");
        }
    } catch(e) {
        log("Got exception: " + e);
    }
    finishSWTest();
}

test();
