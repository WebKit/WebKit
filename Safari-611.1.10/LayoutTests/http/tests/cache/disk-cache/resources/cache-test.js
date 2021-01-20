window.jsTestIsAsync = true;

if (location.protocol != "http:" || location.host != "127.0.0.1:8000") {
    testFailed("This test must be run from http://127.0.0.1:8000");
    finishJSTest();
}

if (!window.internals) {
    testFailed("This test requires window.internals");
    finishJSTest();
}

function getServerDate()
{
    var req = new XMLHttpRequest();
    var t0 = new Date().getTime();
    req.open('GET', "/cache/resources/current-time.cgi", false /* blocking */);
    req.send();
    var serverToClientTime = (new Date().getTime() - t0) / 2;
    if (req.status != 200) {
        console.log("unexpected status code " + req.status + ", expected 200.");
        return new Date();
    }
    return new Date((parseInt(req.responseText) * 1000) + serverToClientTime);
}

var serverClientTimeDelta = getServerDate().getTime() - new Date().getTime();

var uniqueIdCounter = 0;
function makeHeaderValue(value)
{
    if (value == 'now(0)')
        return (new Date(new Date().getTime() + serverClientTimeDelta)).toUTCString();
    if (value == 'now(100)')
        return (new Date(new Date().getTime() + serverClientTimeDelta + 100 * 1000)).toUTCString();
    if (value == 'now(-1000)')
        return (new Date(new Date().getTime() - serverClientTimeDelta - 1000 * 1000)).toUTCString()
    if (value == 'unique()')
        return "" + uniqueIdCounter++;
    return value;
}

function generateTestURL(test)
{
    var body = typeof test.body !== 'undefined' ? escape(test.body) : "";
    var expiresInFutureIn304 = typeof test.expiresInFutureIn304 !== 'undefined' ? test.expiresInFutureIn304 : false;
    var uniqueTestId = Math.floor((Math.random() * 1000000000000));
    var testURL = "resources/generate-response.cgi?body=" + body;
    if (expiresInFutureIn304)
        testURL += "&expires-in-future-in-304=1";
    if (test.delay)
        testURL += "&delay=" + test.delay;
    testURL += "&uniqueId=" + uniqueTestId++;
    if (!test.responseHeaders || !test.responseHeaders["Content-Type"])
        testURL += "&Content-Type=text/plain";
    for (var header in test.responseHeaders)
        testURL += '&' + header + '=' + escape(makeHeaderValue(test.responseHeaders[header]));
    return testURL;
}

function loadResource(test, onload)
{
    if (!test.url)
        test.url = generateTestURL(test);

    test.xhr = new XMLHttpRequest();
    test.xhr.onload = onload;
    test.xhr.onerror = onload;
    test.xhr.onabort = onload;
    test.xhr.open("get", test.url, true);

    for (var header in test.requestHeaders)
        test.xhr.setRequestHeader(header, makeHeaderValue(test.requestHeaders[header]));

    test.xhr.send();
}

function loadResourcesWithOptions(tests, options, completion)
{
    if (options["ClearMemoryCache"])
        internals.clearMemoryCache();
    internals.setStrictRawResourceValidationPolicyDisabled(options["SubresourceValidationPolicy"]);

    var pendingCount = tests.length;
    for (var i = 0; i < tests.length; ++i) {
        loadResource(tests[i], function (ev) {
            --pendingCount;
            if (!pendingCount)
                completion(ev);
        });
    }
}

function loadResources(tests, completion)
{
    loadResourcesWithOptions(tests, { "ClearMemoryCache" : true }, completion);
}

function printResults(tests)
{
    for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];
        debug("response headers: " + JSON.stringify(test.responseHeaders));
        if (test.expiresInFutureIn304)
            debug("response's 'Expires' header is overriden by future date in 304 response");
        if (test.requestHeaders)
            debug("request headers: " + JSON.stringify(test.requestHeaders));
        responseSource = internals.xhrResponseSource(test.xhr);
        debug("response source: " + responseSource);
        debug("");
    }
}

function runTestsNow(tests, completionHandler)
{
    loadResources(tests, function () {
        // Wait a bit so things settle down in the disk cache.
        setTimeout(function () {
            debug("--------Testing loads from disk cache--------");
            loadResourcesWithOptions(tests, { "ClearMemoryCache" : true }, function () {
                printResults(tests);
                debug("--------Testing loads through memory cache (XHR behavior)--------");
                loadResourcesWithOptions(tests, { }, function () {
                    printResults(tests);
                    debug("--------Testing loads through memory cache (subresource behavior)--------");
                    loadResourcesWithOptions(tests, { "SubresourceValidationPolicy": true }, function () {
                        printResults(tests);
                        if (completionHandler)
                            completionHandler();
                        else
                            finishJSTest();
                    });
                });
            });
        }, 100);
    });
}

function runTests(tests, completionHandler)
{
    if (document.readyState == 'complete') {
        runTestsNow(tests, completionHandler);
        return;
    }

    // We need to wait for the load event to have fired because CachedResourceLoader::determineRevalidationPolicy()
    // agressively reuses resources until the load event has fired.
    addEventListener("load", () => {
        runTestsNow(tests, completionHandler);
    });
}

function mergeFields(field, componentField)
{
    for (var name in componentField) {
        if (field[name])
            field[name] += ", " + componentField[name];
        else
            field[name] = componentField[name];
    }
}

function generateTests(testMatrix, includeBody)
{
    includeBody = typeof includeBody !== 'undefined' ? includeBody : true;
    var tests = [];

    var testCount = 1;
    for (var i = 0; i < testMatrix.length; ++i)
        testCount *= testMatrix[i].length;

    for (var testNumber = 0; testNumber < testCount; ++testNumber) {
        var test = {}

        var index = testNumber;
        for (var i = 0; i < testMatrix.length; ++i) {
            var components = testMatrix[i];
            var component = components[index % components.length];
            index = Math.floor(index / components.length);

            for (var field in component) {
                if (!test[field])
                    test[field] = {}
                mergeFields(test[field], component[field]);
            }
        }
        if (includeBody && !test.body)
            test.body = "test body";
        tests.push(test);
    }
    return tests;
}
