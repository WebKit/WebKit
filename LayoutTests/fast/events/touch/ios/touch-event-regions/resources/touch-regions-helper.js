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

function dumpRegions()
{
    if (!window.internals)
        return;

    var resultString = 'touchstart\n';
    var rectList = internals.touchEventRectsForEvent('touchstart');
    resultString += rectsAsString(rectList);

    rectList = internals.touchEventRectsForEvent('touchmove');
    resultString += '\ntouchstart\n' + rectsAsString(rectList);

    rectList = internals.touchEventRectsForEvent('touchend');
    resultString += '\ntouchend\n' + rectsAsString(rectList);

    rectList = internals.touchEventRectsForEvent('touchforcechange');
    resultString += '\ntouchforcechange\n' + rectsAsString(rectList);

    rectList = internals.passiveTouchEventListenerRects();
    resultString += '\npassive\n' + rectsAsString(rectList);

    var resultsPre = document.createElement('pre');
    resultsPre.textContent = resultString;
    document.body.appendChild(resultsPre);
}
