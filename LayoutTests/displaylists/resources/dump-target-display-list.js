if (window.testRunner)
    testRunner.dumpAsText();

var targetDiv;
function doTest()
{
    document.body.offsetWidth;
    targetDiv = document.getElementById('target');
    if (window.internals)
        internals.setElementUsesDisplayListDrawing(targetDiv, true);
    
    if (window.testRunner)
        testRunner.displayAndTrackRepaints();

    if (window.internals) {
        var displayList = internals.displayListForElement(targetDiv);
        document.getElementById('output').textContent = displayList;
    }
}
window.addEventListener('load', doTest, false);
