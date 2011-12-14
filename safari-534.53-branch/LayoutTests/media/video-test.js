
var video = null;
var mediaElement = document; // If not set, an event from any element will trigger a waitForEvent() callback.
var console = null;
var printFullTestDetails = true; // This is optionaly switched of by test whose tested values can differ. (see disableFullTestDetailsPrinting())
var Failed = false;

findMediaElement();
logConsole();

if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
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
    if (!console && document.body) {
        console = document.createElement('div');
        document.body.appendChild(console);
    }
    return console;
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

function testExpected(testFuncString, expected, comparison)
{
    try {
        var observed = eval(testFuncString);
    } catch (ex) {
        consoleWrite(ex);
        return;
    }

    if (comparison === undefined)
        comparison = '==';

    var success = false;
    switch (comparison)
    {
        case '<':   success = observed <  expected; break;
        case '<=': success = observed <= expected; break;
        case '>':   success = observed >  expected; break;
        case '>=': success = observed >= expected; break;
        case '!=':  success = observed != expected; break;
        case '==': success = observed == expected; break;
    }

    reportExpected(success, testFuncString, comparison, expected, observed)
}

var testNumber = 0;

function reportExpected(success, testFuncString, comparison, expected, observed)
{
    testNumber++;

    var msg = "Test " + testNumber;

    if (printFullTestDetails || !success)
        msg = "EXPECTED (<em>" + testFuncString + " </em>" + comparison + " '<em>" + expected + "</em>')";

    if (!success)
        msg +=  ", OBSERVED '<em>" + observed + "</em>'";

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

function waitForEventAndEnd(eventName, funcString)
{
    waitForEvent(eventName, funcString, true)
}

function waitForEvent(eventName, func, endit)
{
    function _eventCallback(event)
    {
        consoleWrite("EVENT(" + eventName + ")");

        if (func)
            func(event);

        if (endit)
            endTest();
    }

    mediaElement.addEventListener(eventName, _eventCallback, true);
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

function testException(testString, exceptionString)
{
    try {
        eval(testString);
    } catch (ex) {
        logResult(ex.code == eval(exceptionString), "TEST(" + testString + ") THROWS("+exceptionString+")");
    }
}

var testEnded = false;

function endTest()
{
    consoleWrite("END OF TEST");
    testEnded = true;
    if (window.layoutTestController)
        layoutTestController.notifyDone();
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
    return url.substr(url.indexOf('/media/')+7);
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
