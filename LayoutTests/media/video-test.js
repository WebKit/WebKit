
var video = null;
var mediaElement = document; // If not set, an event from any element will trigger a waitForEvent() callback.
var consoleElement = null;
var printFullTestDetails = true; // This is optionaly switched of by test whose tested values can differ. (see disableFullTestDetailsPrinting())
var Failed = false;
var Success = true;

var track = null; // Current TextTrack being tested.
var cues = null; // Current TextTrackCueList being tested.
var numberOfTrackTests = 0;
var numberOfTracksLoaded = 0;

findMediaElement();
logConsole();

if (window.testRunner) {
    // Some track element rendering tests require text pixel dump.
    if (typeof requirePixelDump == "undefined")
        testRunner.dumpAsText();

    testRunner.waitUntilDone();
}

function disableFullTestDetailsPrinting()
{
    printFullTestDetails = false;
}

function enableFullTestDetailsPrinting()
{
    printFullTestDetails = true;
}

function logConsole()
{
    if (!consoleElement && document.body) {
        consoleElement = document.createElement('div');
        document.body.appendChild(consoleElement);
    }
    return consoleElement;
}

function findMediaElement()
{
    try {
        video = document.getElementsByTagName('video')[0];
        if (video)
            mediaElement = video;
    } catch (ex) { }
}

function testAndEnd(testFuncString)
{
    test(testFuncString, true);
}

function test(testFuncString, endit)
{
    logResult(eval(testFuncString), "TEST(" + testFuncString + ")");
    if (endit)
        endTest();
}

function compare(testFuncString, expected, comparison)
{
    var observed = eval(testFuncString);

    var success = false;
    switch (comparison)
    {
        case '<':   success = observed <  expected; break;
        case '<=': success = observed <= expected; break;
        case '>':   success = observed >  expected; break;
        case '>=': success = observed >= expected; break;
        case '!=':  success = observed != expected; break;
        case '==': success = observed == expected; break;
        case '===': success = observed === expected; break;
        case 'instanceof': success = observed instanceof expected; break;
    }

    return {success:success, observed:observed};
}

function testExpected(testFuncString, expected, comparison)
{
    if (comparison === undefined)
        comparison = '==';

    try {
        let {success, observed} = compare(testFuncString, expected, comparison);
        reportExpected(success, testFuncString, comparison, expected, observed)
    } catch (ex) {
        consoleWrite(ex);
    }
}

function sleepFor(duration) {
    return new Promise(resolve => {
        setTimeout(resolve, duration);
    });
}

function testExpectedEventually(testFuncString, expected, comparison, timeout)
{
    return new Promise(async resolve => {
        var success;
        var observed;
        var timeSlept = 0;
        if (comparison === undefined)
            comparison = '==';
        while (timeout === undefined || timeSlept < timeout) {
            try {
                let {success, observed} = compare(testFuncString, expected, comparison);
                if (success) {
                    reportExpected(success, testFuncString, comparison, expected, observed);
                    resolve();
                    return;
                }
                await sleepFor(1);
                timeSlept++;
            } catch (ex) {
                consoleWrite(ex);
                resolve();
                return;
            }
        }
        reportExpected(success, testFuncString, comparison, expected, observed, "AFTER TIMEOUT");
        resolve();
    });
}

function testArraysEqual(testFuncString, expected)
{
    var observed;
    try {
        observed = eval(testFuncString);
    } catch (ex) {
        consoleWrite(ex);
        return;
    }

    testExpected(testFuncString + ".length", expected.length);

    for (var i = 0; i < observed.length; i++) {
        testExpected(testFuncString + "[" + i + "]", expected[i]);
    }
}

var testNumber = 0;

function reportExpected(success, testFuncString, comparison, expected, observed, explanation)
{
    testNumber++;

    var msg = "Test " + testNumber;

    if (printFullTestDetails || !success)
        msg = "EXPECTED (<em>" + testFuncString + " </em>" + comparison + " '<em>" + expected + "</em>')";

    if (!success) {
        msg +=  ", OBSERVED '<em>" + observed + "</em>'";
        if (explanation !== undefined)
            msg += ", " + explanation;
    }

    logResult(success, msg);
}

function runSilently(testFuncString)
{
    if (printFullTestDetails)
        consoleWrite("RUN(" + testFuncString + ")");
    try {
        eval(testFuncString);
    } catch (ex) {
        if (!printFullTestDetails) {
            // No details were printed previous, give some now.
            // This will be helpful in case of error.
            logResult(Failed, "Error in RUN(" + testFuncString + "):");
        }
        logResult(Failed, "<span style='color:red'>"+ex+"</span>");
    }
}

function run(testFuncString)
{
    consoleWrite("RUN(" + testFuncString + ")");
    try {
        eval(testFuncString);
    } catch (ex) {
        consoleWrite(ex);
    }
}

function waitForEventWithTimeout(element, type, time, message) {
    let listener = new Promise(resolve => {
        element.addEventListener(type, event => {
            resolve(event);
        }, { once: true });
    });
    let timeout = new Promise((resolve) => {
        setTimeout(resolve, time, 'timeout');
    });

    return Promise.race([
        listener, 
        timeout,
    ]).then(result => {
        if (result === 'timeout') {
            Promise.reject(new Error(message));
            return;
        }
        
        consoleWrite(`EVENT(${result.type})`);
        Promise.resolve(result);
    });
}

function waitFor(element, type, silent) {
    return new Promise(resolve => {
        element.addEventListener(type, event => {
            if (!silent)
                consoleWrite(`EVENT(${event.type})`);
            resolve(event);
        }, { once: true });
    });
}

function waitForEventOnce(eventName, func, endit)
{
    waitForEvent(eventName, func, endit, true)
}

function waitForEventAndEnd(eventName, funcString)
{
    waitForEvent(eventName, funcString, true)
}

function waitForEvent(eventName, func, endit, oneTimeOnly, element)
{
    if (!element)
        element = mediaElement;

    function _eventCallback(event)
    {
        if (oneTimeOnly)
            element.removeEventListener(eventName, _eventCallback, true);

        consoleWrite("EVENT(" + eventName + ")");

        if (func)
            func(event);

        if (endit)
            endTest();
    }

    element.addEventListener(eventName, _eventCallback, true);
}

function waitForEventTestAndEnd(eventName, testFuncString)
{
    waitForEventAndTest(eventName, testFuncString, true);
}

function waitForEventAndFail(eventName)
{
    waitForEventAndTest(eventName, "false", true);
}

function waitForEventAndTest(eventName, testFuncString, endit)
{
    function _eventCallback(event)
    {
        logResult(eval(testFuncString), "EVENT(" + eventName + ") TEST(" + testFuncString + ")");
        if (endit)
            endTest();
    }

    mediaElement.addEventListener(eventName, _eventCallback, true);
}

function waitForEventOnceOn(element, eventName, func, endit)
{
    waitForEventOn(element, eventName, func, endit, true);
}

function waitForEventOn(element, eventName, func, endit, oneTimeOnly)
{
    waitForEvent(eventName, func, endit, oneTimeOnly, element);
}

function testDOMException(testString, exceptionString)
{
    try {
        eval(testString);
    } catch (ex) {
        var exception = ex;
    }
    logResult(exception instanceof DOMException && exception.code === eval(exceptionString),
        "TEST(" + testString + ") THROWS(" + exceptionString + ")");
}

function testException(testString, exceptionString) {
    try {
        eval(testString);
    } catch (ex) {
        var exception = ex;
    }
    logResult(exception !== undefined && exception == eval(exceptionString),
        "TEST(" + testString + ") THROWS(" + exceptionString + ")");
}

var testEnded = false;

function endTest()
{
    consoleWrite("END OF TEST");
    testEnded = true;
    if (window.testRunner) {
        // FIXME (121170): We shouldn't need the zero-delay timer. But text track layout
        // happens asynchronously, so we need it to run first to have stable test results.
        setTimeout("testRunner.notifyDone()", 0);
    }
}

function endTestLater()
{
    setTimeout(endTest, 250);
}

function failTestIn(ms)
{
    setTimeout(function () {
        consoleWrite("FAIL: did not end fast enough");
        endTest();
    }, ms);
}

function failTest(text)
{
    logResult(Failed, text);
    endTest();
}

function passTest(text)
{
    logResult(Success, text);
    endTest();
}

function logResult(success, text)
{
    if (success)
        consoleWrite(text + " <span style='color:green'>OK</span>");
    else
        consoleWrite(text + " <span style='color:red'>FAIL</span>");
}

function consoleWrite(text)
{
    if (testEnded)
        return;
    logConsole().innerHTML += text + "<br>";
}

function relativeURL(url)
{
    return url.substr(url.lastIndexOf('/media/')+7);
}


function isInTimeRanges(ranges, time)
{
    var i = 0;
    for (i = 0; i < ranges.length; ++i) {
        if (time >= ranges.start(i) && time <= ranges.end(i)) {
          return true;
        }
    }
    return false;
}

function testTracks(expected)
{
    tracks = video.textTracks;
    testExpected("tracks.length", expected.length);

    for (var i = 0; i < tracks.length; i++) {
        consoleWrite("<br>*** Testing text track " + i);

        track = tracks[i];
        for (j = 0; j < expected.tests.length; j++) {
            var test = expected.tests[j];
            testExpected("track." + test.property, test.values[i]);
        }
    }
}

function testCues(index, expected)
{
    consoleWrite("<br>*** Testing text track " + index);

    cues = video.textTracks[index].cues;
    testExpected("cues.length", expected.length);
    for (var i = 0; i < cues.length; i++) {
        for (j = 0; j < expected.tests.length; j++) {
            var test = expected.tests[j];
            var propertyString = "cues[" + i + "]." + test.property;
            var propertyValue = eval(propertyString);
            if (test["precision"] && typeof(propertyValue) == 'number')
                propertyValue = propertyValue.toFixed(test["precision"]);
            reportExpected(test.values[i] == propertyValue, propertyString, "==", test.values[i], propertyValue)
        }
    }
}

function allTestsEnded()
{
    numberOfTrackTests--;
    if (numberOfTrackTests == 0)
        endTest();
}

function enableAllTextTracks()
{
    findMediaElement();
    for (var i = 0; i < video.textTracks.length; i++) {
        if (video.textTracks[i].mode == "disabled")
            video.textTracks[i].mode = "hidden";
    }
}

function waitForEventsAndCall(eventList, func)
{
    var requiredEvents = []

    function _eventCallback(event)
    {
        if (!requiredEvents.length)
            return;

        for (var index = 0; index < requiredEvents.length; index++) {
            if (requiredEvents[index][1] === event.type) {
                break;
            }
        }
        if (index >= requiredEvents.length)
            return;

        requiredEvents[index][0].removeEventListener(event, _eventCallback);
        requiredEvents.splice(index, 1);
        if (requiredEvents.length)
            return;

        func();
    }

    for (var i = 0; i < eventList.length; i++) {
        requiredEvents[i] = eventList[i].slice(0);
        requiredEvents[i][0].addEventListener(requiredEvents[i][1], _eventCallback, true);
    }
}

function setCaptionDisplayMode(mode)
{
    if (window.internals)
        internals.setCaptionDisplayMode(mode);
    else
        consoleWrite("<br><b>** This test only works in DRT! **<" + "/b><br>");
}

function runWithKeyDown(fn, preventDefault) 
{
    var eventName = 'keypress'
    function thunk(event) {
        if (preventDefault && event !== undefined)
            event.preventDefault();

        document.removeEventListener(eventName, thunk, false);
        if (typeof fn === 'function')
            fn();
        else
            run(fn);
    }
    document.addEventListener(eventName, thunk, false);

    if (window.internals)
        internals.withUserGesture(thunk);
}

function shouldResolve(promise) {
    return new Promise((resolve, reject) => {
        promise.then(result => {
            logResult(Success, 'Promise resolved');
            resolve(result);
        }).catch((error) => {
            logResult(Failed, 'Promise rejected');
            reject(error);
        });
    });
}

function shouldReject(promise) {
    return new Promise((resolve, reject) => {
        promise.then(result => {
            logResult(Failed, 'Promise resolved incorrectly');
            reject(result);
        }).catch((error) => {
            logResult(Success, 'Promise rejected correctly');
            resolve(error);
        });
    });

}

function handlePromise(promise) {
    function handle() { }
    return promise.then(handle, handle);
}

function checkMediaCapabilitiesInfo(info, expectedSupported, expectedSmooth, expectedPowerEfficient) {
    logResult(info.supported == expectedSupported, "info.supported == " + expectedSupported);
    logResult(info.smooth == expectedSmooth, "info.smooth == " + expectedSmooth);
    logResult(info.powerEfficient == expectedPowerEfficient, "info.powerEfficient == " + expectedPowerEfficient);
}
