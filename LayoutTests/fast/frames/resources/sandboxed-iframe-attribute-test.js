if (window.testRunner)
    window.testRunner.dumpAsText();

var failed = false;
var frames = 0;         /* number of allowed frames that called back */

function fail(message)
{
    setStatus("FAIL: " + message);
    failed = true;
}

function setStatus(status)
{
    if (!document.getElementById("testStatus")) {
        var div = document.createElement('div');
        div.id = "testStatus";
        document.body.appendChild(div);
    }
    document.getElementById("testStatus").innerHTML = status;
}

function allowedCallFromSandbox()
{
    ++frames;
}

function disallowedCallFromSandbox()
{
    fail("disallowed script executed");
}

function disallowedFormSubmitted()
{
    fail("sandboxing failed: form submitted in sandboxed frame");
}

window.onload = function()
{
    var expected = document.querySelectorAll('iframe[src="resources/sandboxed-iframe-attribute-parsing-allowed.html"]').length;
    if (frames == expected && !failed)
        setStatus("PASS");
    else if (!failed)
        fail("scripting disabled in one or more frames where it should be enabled");
}
