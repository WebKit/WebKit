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
    if (value == 'unique()')
        return "" + uniqueIdCounter++;
    return value;
}

function generateTestURL(test, includeBody)
{
    includeBody = typeof includeBody !== 'undefined' ? includeBody : true;
    var uniqueTestId = Math.floor((Math.random() * 1000000000000));
    var cgi_script = "resources/generate-response.cgi?include-body=" + (includeBody ? "1" : "0");
    var testURL = cgi_script + "&uniqueId=" + uniqueTestId++ + "&Content-type=text/plain";
    for (var header in test.responseHeaders)
        testURL += '&' + header + '=' + makeHeaderValue(test.responseHeaders[header]);
    return testURL;
}

function loadResource(test, onload)
{
    if (!test.url)
        test.url = generateTestURL(test, test.includeBody);

    test.xhr = new XMLHttpRequest();
    test.xhr.onload = onload;
    test.xhr.open("get", test.url, true);

    for (var header in test.requestHeaders)
        test.xhr.setRequestHeader(header, makeHeaderValue(test.requestHeaders[header]));

    test.xhr.send();
}

function loadResources(tests, completetion)
{
    var pendingCount = tests.length;
    for (var i = 0; i < tests.length; ++i) {
        loadResource(tests[i], function (ev) {
            --pendingCount;
            if (!pendingCount)
                completetion();
         });
    }
}

function printResults(tests)
{
    for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];
        debug("response headers: " + JSON.stringify(test.responseHeaders));
        if (test.requestHeaders)
            debug("request headers: " + JSON.stringify(test.requestHeaders));
        responseSource = internals.xhrResponseSource(test.xhr);
        debug("response source: " + responseSource);
        debug("");
    }
}

function runTests(tests, completionHandler)
{
    loadResources(tests, function () {
        // Otherwise we just get responses from the memory cache.
        internals.clearMemoryCache();
        // Wait a bit so things settle down in the disk cache.
        setTimeout(function () {
            loadResources(tests, function () {
                printResults(tests);
                if (completionHandler)
                    completionHandler();
                else
                    finishJSTest();
            });
        }, 100);
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
        test.includeBody = includeBody;
        tests.push(test);
    }
    return tests;
}
