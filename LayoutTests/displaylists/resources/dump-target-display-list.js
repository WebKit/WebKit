if (window.testRunner)
    testRunner.dumpAsText();

var targetDiv;
async function doTest()
{
    testRunner?.waitUntilDone();
    document.body.offsetWidth;
    targetDiv = document.getElementById('target');
    if (window.internals)
        internals.setElementUsesDisplayListDrawing(targetDiv, true);
    
    await testRunner?.displayAndTrackRepaints();

    if (window.internals) {
        var displayList = internals.displayListForElement(targetDiv);
        document.getElementById('output').textContent = displayList;
    }
    testRunner?.notifyDone();
}
window.addEventListener('load', doTest, false);
