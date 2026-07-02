async function test()
{
    let frameLoadedCount = 0;
    const expectedFrameLoadedCount = 3;

    function addTestFrame(url, onloadFunction)
    {
        const iframe = frame.contentDocument.createElement("iframe");
        iframe.src = url;
        const iframeTimeoutHandle = setInterval(() => {
            if (iframe.contentDocument.URL != "" && iframe.contentDocument.URL != "about:blank") {
                clearTimeout(iframeTimeoutHandle);
                onloadFunction();
            }
        }, 100);
        iframe.onload = () => {
            clearTimeout(iframeTimeoutHandle);
            onloadFunction();
        };
        frame.contentDocument.body.appendChild(iframe);
        return iframe;
    }

    frameText = function(testFrame)
    {
        let innerText = testFrame.contentDocument.body.innerText;
        const newLineIndex = innerText.indexOf('\n');
        if (newLineIndex > 0)
            innerText = innerText.slice(0, newLineIndex);
        return innerText;
    }

    function testFrame4Loaded()
    {
        // Currently goes to the network because the service worker registration is inert.
        shouldBeEqualToString("frameText(testFrame4)", "Success!");

        frame.remove();
        finishJSTest();
    }

    function frameLoaded()
    {
        if (++frameLoadedCount < expectedFrameLoadedCount)
            return;

        shouldBeEqualToString("frameText(testFrame1)", "Timeout resource fetched from network"); // Load should fall back to the network.
        shouldBeEqualToString("frameText(testFrame2)", "Not Found"); // Load should have went to network and failed with 404.
        shouldBeEqualToString("frameText(testFrame3)", "Success!"); // Load should fall back to the network.

        // Now we can fetch that same URL again, which *could* relaunch the service worker and handle it there, but for now this service worker registration is inert and fetches through it will go to the network instead.
        // I'm leaving this in to cover future cases where we do relaunch the SW to handle it.
        if (window.testRunner)
            testRunner.setServiceWorkerFetchTimeout(60);
        testFrame4 = addTestFrame("succeed-fallback-check.py", testFrame4Loaded);
    }

    description("Tests various navigation time out cases for service workers.");
    jsTestIsAsync = true;
 
    try {
        frame = await interceptedFrame("resources/basic-timeout-worker.js", "/workers/service/resources/");

        if (window.testRunner)
            testRunner.setServiceWorkerFetchTimeout(0);
       
        // The following two loads should time out immediately.
        testFrame1 = addTestFrame("timeout-fallback.html", frameLoaded);
        testFrame2 = addTestFrame("timeout-no-fallback.html", frameLoaded);

        // This load would have been properly handled by the service worker but will go to network anyway
        // because the service worker was hung by the previous loads.
        testFrame3 = addTestFrame("succeed-fallback-check.py", frameLoaded);
    } catch(e) {
        log("Got exception: " + e);
        finishJSTest();
    }
}

test();
