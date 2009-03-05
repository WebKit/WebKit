if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}
var video;
var media;
var console = document.createElement('div');
document.body.appendChild(console);
try {
    video = document.getElementsByTagName('video')[0];
    if (video)
        media = video;
} catch (ex) { }

function hanged()
{
    consoleWrite("FAIL: timed out");
    if (window.layoutTestController)
        layoutTestController.notifyDone();  
}
setTimeout(hanged, 10000);

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
        case '>':  success = observed >  expected; break;
        case '!=': success = observed != expected; break;
        case '==': success = observed == expected; break;
    }
    
    reportExpected(success, testFuncString, comparison, expected, observed)
}

function reportExpected(success, testFuncString, comparison, expected, observed)
{
    var msg = "EXPECTED (<em>" + testFuncString + " </em>" + comparison + " '<em>" + expected + "</em>')";
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

    media.addEventListener(eventName, _eventCallback);    
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
    
    media.addEventListener(eventName, _eventCallback);
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
    console.innerHTML += text + "<br>";
}

function relativeURL(url)
{
    return url.substr(url.indexOf('/media/')+7);
}