
function testZoomAfterTap(targetElement, xOffset, yOffset)
{
    if (!window.testRunner || !testRunner.runUIScript)
        return;

    var point = getPointInsideElement(targetElement, xOffset, yOffset);

    var uiScript = zoomAfterSingleTapUIScript(point.x, point.y);
    testRunner.runUIScript(uiScript, function(result) {
        var results = tableFromJSON(result);
        document.body.appendChild(results);
        testRunner.notifyDone();
    });
}

function zoomAfterSingleTapUIScript(x, y)
{
    return `
        (function() {
            uiController.didEndZoomingCallback = function() {
                var result = {
                    'tap location' : { 'x' : ${x}, 'y' : ${y}},
                    'scale' : uiController.zoomScale,
                    'visibleRect' : uiController.contentVisibleRect
                };

                var result = JSON.stringify(result, function(key, value) {
                      if (typeof value === "number")
                          return value.toFixed(3);
                    return value;
                });

                uiController.uiScriptComplete(result);
            };

            uiController.singleTapAtPoint(${x}, ${y}, function() {});
        })();`
}

function getPointInsideElement(element, xOffset, yOffset)
{
    var clientRect = element.getBoundingClientRect();

    var scrollLeft = document.scrollingElement.scrollLeft;
    var scrollTop = document.scrollingElement.scrollTop;
    
    // Returns point in document coordinates.
    return { 'x' : clientRect.left + scrollLeft + xOffset, 'y' : clientRect.top + scrollTop + yOffset };
}

function tableFromJSON(value)
{
    var result = JSON.parse(value);
    
    var table = document.createElement('table');
    for (var property in result) {
        var row = document.createElement('tr');
        
        var th = document.createElement('th');
        th.textContent = property;
        
        var td = document.createElement('td');
        var value = result[property];
        if (typeof value === "object")
            value = JSON.stringify(value, "", 2).replace(/"/g, '');
        
        td.textContent = value;
    
        row.appendChild(th);
        row.appendChild(td);
        table.appendChild(row);
    }
    
    return table;
}
