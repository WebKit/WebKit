if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

var currentTest;
var tests;
var uniqueId;
var now;

var loadedFrameCount = 0;

var frame1 = document.createElement('iframe');
frame1.setAttribute('style', 'visibility:hidden;width:0;height:0');
frame1.setAttribute('src', 'resources/test-frame.html');
document.body.appendChild(frame1);
frame1.onload = function () { loadedFrameCount++; }

var frame2 = document.createElement('iframe');
frame2.setAttribute('style', 'visibility:hidden;width:0;height:0');
frame2.setAttribute('src', 'resources/test-frame.html');
document.body.appendChild(frame2);

var consoleDiv = document.createElement('div');
document.body.appendChild(consoleDiv);
frame2.onload = function () { loadedFrameCount++; }

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

function loadTestFrame(frame, testSpec)
{
    var first = true;
    var queryString = 'cache-simulator.cgi?uniqueId=' + uniqueId + '&';
    for (var header in testSpec.testHeaders) {
        if (!first)
            queryString += '&';
        var value = testSpec.testHeaders[header];
        if (value == '[now+10s]')
            value = new Date(now.getTime() + 10 * 1000).toUTCString();
        else if (value == '[now-10s]')
            value = new Date(now.getTime() - 10 * 1000).toUTCString();
        else if (value == '[now+3600s]')
            value = new Date(now.getTime() + 3600 * 1000).toUTCString();
        else if (value == '[now-3600s]')
            value = new Date(now.getTime() - 3600 * 1000).toUTCString();
        else if (value == '[now]')
            value = new Date(now.getTime()).toUTCString();
        queryString += header + '=' + value;
        first = false;
    }
    var doc = frame.contentDocument;
    var head = doc.getElementsByTagName('head')[0];
    var scr = doc.createElement('script');
    head.appendChild(scr);
    scr.onload = function () { setTimeout(function () { frameLoaded(frame, testSpec); }, 0); }
    scr.setAttribute('src', queryString);
}

function frameLoaded(frame, testSpec)
{
    if (frame == frame1) {
        setTimeout(function () { loadTestFrame(frame2, testSpec) }, testSpec.delay * 1000);
        return;
    }
    testComplete(testSpec);
}

function nextTest()
{
    var testSpec = tests[currentTest];
    uniqueId = Math.floor(100000000 * Math.random());
    now = new Date(new Date().getTime() + serverClientTimeDelta);
    if (!testSpec) {
        if (window.testRunner)
            testRunner.notifyDone();
        return;
    }
    loadTestFrame(frame1, testSpec);
    currentTest++;
}

function testComplete(testSpec)
{
    var line = document.createElement('div');
    var result = frame1.contentWindow.randomNumber == frame2.contentWindow.randomNumber ? 'Cached' : 'Uncached';
    var passed = result == testSpec.expectedResult;

    if (testSpec.description)
        line.innerHTML += testSpec.description;
    else {
        for (var header in testSpec.testHeaders)
            line.innerHTML += header + ": " + testSpec.testHeaders[header] + "; ";
    }
    if (testSpec.delay)
        line.innerHTML += "[delay=" + testSpec.delay + "s] "
    line.innerHTML += "  (result=" + result + " expected=" + testSpec.expectedResult + ") ";
    line.innerHTML += passed ? "<font color=green>PASS</font> " : "<font color=red>FAIL</font> "

    consoleDiv.appendChild(line);

    nextTest();
}

function runTests()
{
    currentTest = 0;
    if (loadedFrameCount==2)
        nextTest();
    else
        setTimeout(runTests, 100);
}
