if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}
var watchID = -1;
var passedPosition = null;
var console = document.createElement('div');
document.body.appendChild(console);

function reset()
{
    if (window.layoutTestController) {
        layoutTestController.notifyDone();  
        watchID = -1;
        passedPosition = null;
    }
}

function hanged()
{
    consoleWrite("FAIL: timed out");
    reset();
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

function testExpected(testFuncString, expected)
{
    try {
        var observed = eval(testFuncString);
    } catch (ex) {
        consoleWrite(ex);
        return;
    }
    
    var msg = "expected " + testFuncString + "=='" + expected + "', observed '" + observed + "'";

    if (observed == expected)
        consoleWrite(msg + " - <span style='color:green'>OK</span>");
    else
        consoleWrite(msg + " - <span style='color:red'>FAIL</span>");
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

function watchPositionAndEnd(funcString)
{
    watchPosition(funcString, true)
}

function watchPosition(func, endit)
{
    function _positionCallback(position)
    {        
        passedPosition = position;
        test("passedPosition == navigator.geolocation.lastPosition");
        
        consoleWrite("POSITION(" + position + ")");
        
        if (func)
            func();
        
        if (endit)
            endTest();    
    }
    
    watchID = navigator.geolocation.watchPosition(_positionCallback);
    test("watchID > 0");
}

function watchPositionTestAndEnd(testFuncString)
{
    watchPositionAndTest(testFuncString, true);
}

function watchPositionAndFail()
{
    watchPositionAndTest("false", true);
}

function watchPositionAndTest(testFuncString, endit)
{
    function _positionCallback(position)
    {
        passedPosition = position;
        
        if (eval(testFuncString))
            consoleWrite("POSITION(" + position + ") TEST(" + testFuncString + ") <span style='color:green'>OK</span>");
        else
            consoleWrite("POSITION(" + position + ") TEST(" + testFuncString + ") <span style='color:red'>FAIL</span>");

        if (endit)
            endTest();    
    }
    
    watchID = navigator.geolocation.watchPosition(_positionCallback);
    test("watchID > 0");
}

function getCurrentPositionAndEnd(funcString)
{
    getCurrentPosition(funcString, true)
}

function getCurrentPosition(func, endit)
{
    function _positionCallback(position)
    {        
        passedPosition = position;
        
        consoleWrite("POSITION(" + position + ")");
        
        if (func)
            func();
        
        if (endit)
            endTest();    
    }
    
    navigator.geolocation.getCurrentPosition(_positionCallback);
}

function getCurrentPositionTestAndEnd(testFuncString)
{
    getCurrentPositionAndTest(testFuncString, true);
}

function getCurrentPositionAndFail()
{
    getCurrentPositionAndTest("false", true);
}

function getCurrentPositionAndTest(testFuncString, endit)
{
    function _positionCallback(position)
    {
        passedPosition = position;
        
        if (eval(testFuncString))
            consoleWrite("POSITION(" + position + ") TEST(" + testFuncString + ") <span style='color:green'>OK</span>");
        else
            consoleWrite("POSITION(" + position + ") TEST(" + testFuncString + ") <span style='color:red'>FAIL</span>");
        
        if (endit)
            endTest();    
    }
    
    navigator.geolocation.getCurrentPosition(_positionCallback);
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
    reset();
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

function consoleWrite(text)
{
    if (testEnded)
        return;
    console.innerHTML += text + "<br>";
}

function relativeURL(url)
{
    return url.substr(url.indexOf('/geolocation/')+13);
}