
    window.jsTestIsAsync = true;
    
    window.addEventListener("load", () => {

        if (window.testRunner) {
            testRunner.waitUntilDone();
            testRunner.setUserMediaPermission(true);
            testRunner.dumpAsText();
            testRunner.dumpChildFramesAsText(true);
        }
        if (window.internals)
            internals.settings.setMediaCaptureRequiresSecureConnection(true);

        if (location.protocol != testInfo.mainFrame.protocol)
            window.location = createURL(new URL(window.location.href).pathname, testInfo.mainFrame);
        else {
            debug(`URL: ${window.location.href}`);
            let iframe = document.createElement("iframe");
            let search = testInfo.shouldSucceed ? "succeed" : "fail"
            iframe.src = `${createURL(testInfo.iFrame.pathname, testInfo.iFrame)}?${search};${testInfo.depth}`;
            document.body.appendChild(iframe);
        }
    });

    function createURL(pathName, info)
    {
        let port = info.port != 0 ? `:${info.port}` : '';
        return `${info.protocol}//${info.host}${port}${pathName}`;
    }