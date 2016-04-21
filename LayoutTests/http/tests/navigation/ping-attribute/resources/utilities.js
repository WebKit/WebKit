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
    xhr.open("GET", "resources/delete-ping.php", true /* async */);
    xhr.send(null);
    xhr.onload = callback;
    xhr.onerror = done;
}

function clickElement(element)
{
    if (!window.eventSender)
        return;
    eventSender.mouseMoveTo(element.offsetLeft + 2, element.offsetTop + 2);
    eventSender.mouseDown();
    eventSender.mouseUp();
}
