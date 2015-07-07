function checkIfFrameLocationMatchesSrcAndCallDone(frameId)
{
    if (!window.testRunner)
        return;

    var actualURL = 'unavailable', frame = document.getElementById(frameId); 
    try {
        actualURL = frame.contentWindow.location.href;
    } 
    catch (e) {}
    
    if (actualURL != frame.src)
        alert('URL mismatch: ' + actualURL + ' vs. ' + frame.src);

    testRunner.notifyDone();
}

function sendRequestFromIFrame(url, params, HTTPMethod, callbackWhenDone)
{
    if (!params || !params.length)
        return;
        
    if (!HTTPMethod)
        HTTPMethod = 'GET';
        
    if (document.getElementById('frame'))
        document.body.removeChild(document.getElementById('frame'));
    var iFrameObj = document.createElement('iframe');
    iFrameObj.id = 'frame';
    document.body.appendChild(iFrameObj);
    var frameContent = iFrameObj.contentDocument;
    frameContent.open();
    frameContent.write('<form method="' + HTTPMethod + '" name="form" id="form" action="' + url + '">');
    if (params.length > 0) {
        var paramArray = params.split('&');
        for (var i = 0; i < paramArray.length; ++i) {
            var paramElement = paramArray[i].split('=', 2);
            frameContent.write('<input type="text" name="' + paramElement[0] + '" value="' + paramElement[1] + '">');
        }   
    }
    frameContent.write('</form>');
    frameContent.close();
    if (callbackWhenDone)
        iFrameObj.onload = callbackWhenDone;
    frameContent.getElementById('form').submit();
}


function notifyDoneAfterReceivingBeforeloadFromIds(ids)
{
    var loadAttempted = 0;
    window.addEventListener("message", function(event) {
        var index = ids.indexOf(event.data);
        if (index == -1)
            return;

        loadAttempted = loadAttempted | (1 << index);
        if (loadAttempted == (1 << ids.length) - 1)
            testRunner.notifyDone();
    }, false);
}

