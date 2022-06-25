if (window.testRunner)
    testRunner.dumpAsText();

window.onload = function ()
{
    if (!window.testRunner || !window.internals)
        return;

    var userAgentShadowRoot = internals.ensureUserAgentShadowRoot(document.getElementById("shadow-host"));
    var script = document.createElement("script");
    userAgentShadowRoot.appendChild(script);

    script.textContent = "testPassed()";
}

function testPassed()
{
    document.getElementById("result").textContent = "PASS did execute inline script.";
}
