if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}

var video = document.getElementsByTagName('video')[0];
var media;
if (video)
    media = video;
var console = document.createElement('div');
document.body.appendChild(console);

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
    if (eval(testFuncString))
        consoleWrite("TEST(" + testFuncString + ") <span style='color:green'>OK</span>");
    else
        consoleWrite("TEST(" + testFuncString + ") <span style='color:red'>FAIL</span>");    

    if (endit)
        endTest();  
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
            func();
        
        if (endit)
            endTest();    
    }

    media.addEventListener(eventName, _eventCallback);    
}

function waitForEventTestAndEnd(eventName, testFuncString)
{
    waitForEventAndTest(eventName, testFuncString, true);
}

function waitForEventAndTest(eventName, testFuncString, endit)
{
    function _eventCallback(event)
    {
        if (eval(testFuncString))
            consoleWrite("EVENT(" + eventName + ") TEST(" + testFuncString + ") <span style='color:green'>OK</span>");
        else
            consoleWrite("EVENT(" + eventName + ") TEST(" + testFuncString + ") <span style='color:red'>FAIL</span>");
        
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
        if (ex.code == eval(exceptionString))
            consoleWrite("TEST(" + testString + ") THROWS("+exceptionString+") <span style='color:green'>OK</span>");
        else
            consoleWrite("TEST(" + testString + ") THROWS("+exceptionString+") <span style='color:red'>FAIL</span>");    
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