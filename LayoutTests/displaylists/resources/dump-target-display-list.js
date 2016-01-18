if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

var targetDiv;
function doTest()
{
    document.body.offsetWidth;
    targetDiv = document.getElementById('target');
    if (window.internals)
        internals.setElementUsesDisplayListDrawing(targetDiv, true);
    
    window.setTimeout(function() {
        if (window.internals) {
            var displayList = internals.displayListForElement(targetDiv);
            document.getElementById('output').textContent = displayList;
        }
        if (window.testRunner)
            testRunner.notifyDone();
    }, 0);
}
window.addEventListener('load', doTest, false);
