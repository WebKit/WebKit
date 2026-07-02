jsTestIsAsync = true;

function stringFromRect(domRect)
{
    return `${domRect.x}, ${domRect.y} - ${domRect.width} x ${domRect.height}`;
}

var logging = '';
function accumulateLog(s)
{
    logging += s + '\n';
}

function logFixedObject()
{
    var fixedElement = document.getElementById('extent');
    accumulateLog('client rect of fixed object:' + stringFromRect(fixedElement.getBoundingClientRect()));
}

function logFixedAndViewports()
{
    accumulateLog('layoutViewport: ' + stringFromRect(internals.layoutViewportRect()));
    accumulateLog('visualViewport: ' + stringFromRect(internals.visualViewportRect()));
    logFixedObject();
}

function doTest(scriptCompleteCallback)
{
    accumulateLog('Before rotation');
    logFixedAndViewports();
    if (!window.testRunner)
        return;

    testRunner.runUIScript(getRotationUIScript(), function(result) {
        accumulateLog('');
        accumulateLog('After rotation');
        logFixedAndViewports();
        
        var logPre = document.createElement('pre');
        logPre.textContent = logging;
        document.body.appendChild(logPre);

        if (scriptCompleteCallback)
            scriptCompleteCallback();
        else {
            if (window.testRunner)
                testRunner.notifyDone();
        }
    });
}
