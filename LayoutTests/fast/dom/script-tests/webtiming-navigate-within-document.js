description("This test checks that navigating within the document does not reset Web Timing numbers.");

var performance = window.webkitPerformance || {};
var timing = performance.timing || {};

function checkTimingNotChanged()
{
    for (var property in timing) {
        if (timing[property] === initialTiming[property])
            testPassed(property + " is unchanged.");
        else
            testFailed(property + " changed.");
    }
    finishJSTest();
}

var initialTiming = {};
function saveTimingAfterLoad()
{
    for (var property in timing) {
        initialTiming[property] = timing[property];
    }
    window.location.href = "#1";
    setTimeout("checkTimingNotChanged()", 0);
}

function loadHandler()
{
    window.removeEventListener("load", loadHandler);
    setTimeout("saveTimingAfterLoad()", 0);
}
window.addEventListener("load", loadHandler, false);

jsTestIsAsync = true;

var successfullyParsed = true;
