var consoleDiv = null;
var printFullTestDetails = true; // This is optionaly switched of by test whose tested values can differ. (see disableFullTestDetailsPrinting())
var runPixelTests;

logConsole();

if (window.testRunner) {
    testRunner.dumpAsText(runPixelTests);
    testRunner.waitUntilDone();
}

function runWithKeyDown(fn) 
{
    // FIXME: WKTR does not yet support the keyDown() message.  Do a mouseDown here
    // instead until keyDown support is added.
    var eventName = !window.testRunner || eventSender.keyDown ? 'keypress' : 'mousedown'

    function thunk() {
        document.removeEventListener(eventName, thunk, false);
        fn();
    }
    document.addEventListener(eventName, thunk, false);

    if (window.testRunner) {
        if (eventSender.keyDown)
            eventSender.keyDown(" ", []);
        else
            eventSender.mouseDown();
    }
}

function logConsole()
{
    if (!consoleDiv && document.body) {
        consoleDiv = document.createElement('div');
        document.body.appendChild(consoleDiv);
    }
    return consoleDiv;
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
        case '<':  success = observed <  expected; break;
        case '<=': success = observed <= expected; break;
        case '>':  success = observed >  expected; break;
        case '>=': success = observed >= expected; break;
        case '!=': success = observed != expected; break;
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

function run(testFuncString)
{
    consoleWrite("RUN(" + testFuncString + ")");
    try {
        eval(testFuncString);
    } catch (ex) {
        consoleWrite(ex);
    }
}

function waitForEventAndEnd(element, eventName, funcString)
{
    waitForEvent(element, eventName, funcString, true)
}

function waitForEventOnce(element, eventName, func, endit)
{
    waitForEvent(element, eventName, func, endit, true)
}

function waitForEvent(element, eventName, func, endit, once)
{
    function _eventCallback(event)
    {
        if (once)
            element.removeEventListener(eventName, _eventCallback);

        consoleWrite("EVENT(" + eventName + ")");

        if (func)
            func(event);
        
        if (endit)
            endTest();    
    }

    element.addEventListener(eventName, _eventCallback);
}

function waitForEventAndTest(element, eventName, testFuncString, endit)
{
    function _eventCallback(event)
    {
        logResult(eval(testFuncString), "EVENT(" + eventName + ") TEST(" + testFuncString + ")");
        if (endit)
            endTest();    
    }

    element.addEventListener(eventName, _eventCallback);
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

function waitForEventTestAndEnd(element, eventName, testFuncString)
{
    waitForEventAndTest(element, eventName, testFuncString, true);
}

function sleepFor(duration) {
    return new Promise(resolve => {
        setTimeout(resolve, duration);
    });
}

var testEnded = false;

function endTest()
{
    consoleWrite("END OF TEST");
    testEnded = true;
    if (window.testRunner)
        testRunner.notifyDone();     
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


