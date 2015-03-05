function runBlockingTest(validTestSet, blockedTestSet) {
    function getUrlIterator(validTestSet, blockedTestSet) {
        var validTestSetIndex = 0;
        var blockedTestSetIndex = 0;
        return function() {
            if (validTestSetIndex < validTestSet.length)
                return { url:validTestSet[validTestSetIndex++], expectBlock:false};
            if (blockedTestSetIndex < blockedTestSet.length)
                return { url:blockedTestSet[blockedTestSetIndex++], expectBlock:true};
            return;
        }
    }

    function tryLoadingURL(testCase, testFunction) {
        var request = new XMLHttpRequest;
        request.open("GET", testCase.url, true);
        request.testCase = testCase;
        request.timeout = 50;

        var timeoutId = setTimeout( function() { testFunction("timeout", request); }, 50);

        request.addEventListener("readystatechange", function() { testFunction("readystatechange", request, timeoutId); });
        request.addEventListener("error", function() { testFunction("error", request, timeoutId); });
        request.addEventListener("timeout", function() { testFunction("timeout", request, timeoutId); });
        request.send();
    }

    function testFunction(eventType, request, timeoutId)
    {
        isBlocked = true;
        if (eventType === "error" || eventType === "timeout")
            debug("URL: " + request.testCase.url + " is blocked.");
        else if (eventType == "readystatechange") {
            if (request.readyState == XMLHttpRequest.HEADERS_RECEIVED) {
                isBlocked = false;
                debug("URL: " + request.testCase.url + " is not blocked.");
            } else
                return;
        }

        if (request.testCase.expectBlock)
            shouldBeTrue("isBlocked");
        else
            shouldBeFalse("isBlocked");

        if (timeoutId !== undefined)
            clearTimeout(timeoutId);

        runNextTest();
    }

    var urlIterator = getUrlIterator(validTestSet, blockedTestSet);
    function runNextTest() {
        nextCase = urlIterator();
        if (nextCase)
            tryLoadingURL(nextCase, testFunction);
        else
            finishJSTest();
    }

    runNextTest();
}