if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function getUIScript()
{
    return "(function() { \
        var result = { \
            'scale' : uiController.zoomScale, \
            'maxScale' : uiController.maximumZoomScale, \
            'minScale' : uiController.minimumZoomScale, \
            'visibleRect' : uiController.contentVisibleRect \
        }; \
        return JSON.stringify(result, function(key, value) { \
              if (typeof value === \"number\") \
                  return value.toFixed(5); \
            return value; \
        }); \
    })();";
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
            value = JSON.stringify(value);
        
        td.textContent = value;
    
        row.appendChild(th);
        row.appendChild(td);
        table.appendChild(row);
    }
    
    return table;
}

function getViewport()
{
    var metaTags = document.head.querySelectorAll('meta');
    if (!metaTags.length)
        return;

    var metaTag = metaTags[0];
    document.getElementById('viewport').textContent = metaTag.getAttribute('content');
}

function runTest()
{
    getViewport();
    
    if (testRunner.runUIScript) {
        testRunner.runUIScript(getUIScript(), function(resultString) {
            document.getElementById('result').appendChild(tableFromJSON(resultString));
            testRunner.notifyDone();
        });
    }
}
