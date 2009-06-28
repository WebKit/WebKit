function sendRequestFromIFrame(url, params, HTTPMethod)
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
    frameContent.getElementById('form').submit();
}