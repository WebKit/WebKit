if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function nonFastScrollableRects()
{
    var rects = internals.nonFastScrollableRects();
    var result = "";
    for (var i = 0; i < rects.length; ++i) {
        var rect = rects[i];
        if (i)
            result += '\n';
        result += rect.left + ', ' + rect.top + ' - ' + rect.right + ', ' + rect.bottom;
    }
    return result;
}

function dumpNonFastScrollableRects()
{
    if (window.internals)
        document.getElementById('output').textContent = nonFastScrollableRects();

    if (window.testRunner)
        testRunner.notifyDone();    
}

async function dumpRegionAndNotifyDone()
{
    await UIHelper.renderingUpdate();
    if (window.internals) {
        var output = nonFastScrollableRects() + '\n' + internals.layerTreeAsText(document, internals.LAYER_TREE_INCLUDES_EVENT_REGION + internals.LAYER_TREE_INCLUDES_ROOT_LAYER_PROPERTIES);
        document.getElementById('output').textContent = output;
    }

    if (window.testRunner)
        testRunner.notifyDone();
}
