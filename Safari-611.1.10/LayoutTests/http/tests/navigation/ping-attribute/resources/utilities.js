function setCookie()
{
    try {
        var xhr = new XMLHttpRequest;
        xhr.open("GET", "../../cookies/resources/setCookies.cgi", false);
        xhr.setRequestHeader("SET-COOKIE", "hello=world;path=/");
        xhr.send(null);
        if (xhr.status != 200) {
            document.getElementsByTagName("body")[0].appendChild(document.createTextNode("FAILED: cookie not set"));
            if (window.testRunner)
                testRunner.notifyDone();
        }
    } catch (e) {
        document.getElementsByTagName("body")[0].appendChild(document.createTextNode("FAILED: cookie not set"));
        if (window.testRunner)
            testRunner.notifyDone();
    }
}

function clearLastPingResultAndRunTest(callback)
{
    function done()
    {
        if (window.testRunner)
            testRunner.notifyDone();
    }

    var xhr = new XMLHttpRequest;
    xhr.open("GET", "../../resources/delete-ping.php", true /* async */);
    xhr.send(null);
    xhr.onload = callback;
    xhr.onerror = done;
}

function clickElement(element)
{
    var x = element.offsetLeft + 2;
    var y = element.offsetTop + 2;
    var supportsTouchEvents = "ontouchstart" in window;
    if (supportsTouchEvents && window.testRunner && testRunner.runUIScript)
        testRunner.runUIScript("(function() { uiController.singleTapAtPoint(" + x + ", " + y + ", function() { /* Do nothing */ }); })();", function () { /* Do nothing */ });
    else if (window.eventSender) {
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseDown();
        eventSender.mouseUp();
    }
}
