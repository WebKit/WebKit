if (window.testRunner)
    testRunner.dumpAsText();

function rectsAsString(rects)
{
    var result = "";
    for (var i = 0; i < rects.length; ++i) {
        var rect = rects[i];
        if (i)
            result += '\n';
        result += rect.left + ', ' + rect.top + ' - ' + rect.right + ', ' + rect.bottom;
    }
    return result;
}

function dumpRegion()
{
    if (window.internals) {
        var rects = window.internals.nonFastScrollableRects();
        document.getElementById('output').textContent = rectsAsString(rects);
    }
}
