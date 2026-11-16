if (window.testRunner)
    testRunner.dumpAsText();

var targetDiv;
async function doTest()
{
    document.body.offsetWidth;
    targetDiv = document.getElementById('target');
    if (window.internals) {
        internals.setElementUsesDisplayListDrawing(targetDiv, true);
        internals.setElementTracksDisplayListReplay(targetDiv, true);
    }
    
    await testRunner?.displayAndTrackRepaints();

    if (window.internals) {
        var displayList = internals.displayListForElement(targetDiv);
        var replayList = internals.replayDisplayListForElement(targetDiv);
        document.getElementById('output').textContent = 'recorded: ' + displayList + '\n\nreplayed: ' + replayList;
    }
}
window.addEventListener('load', doTest, false);
