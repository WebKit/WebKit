function checkThatPositionErrorCallbackIsCalledWithPositionUnavailableForGeolocationMethod(geolocationMethodName, expectedPositionUnavailableErrorMessage, callbackFunction)
{
    function done()
    {
        if (callbackFunction)
            callbackFunction();
    }

    function logMessage(message)
    {
        document.getElementById("console").appendChild(document.createTextNode(message + "\n"));
    }

    function didReceivePosition()
    {
        logMessage("FAIL should have invoked error callback, but invoked success callback.");
        done();
    }

    function didReceiveError(error)
    {
        if (error.code === error.POSITION_UNAVAILABLE)
            logMessage("PASS error.code is error.POSITION_UNAVAILABLE.");
        else
            logMessage("FAIL error.code should be " + error.POSITION_UNAVAILABLE + ". Was " + error.code + ".");

        if (error.message === expectedPositionUnavailableErrorMessage)
            logMessage('PASS error.message is "' + expectedPositionUnavailableErrorMessage + '".');
        else
            logMessage('FAIL error.message should be "' + expectedPositionUnavailableErrorMessage + '". Was "' + error.message + '".');
        done();
    }

    navigator.geolocation[geolocationMethodName](didReceivePosition, didReceiveError);
}

function markupToCheckThatPositionErrorCallbackIsCalledWithPositionUnavailableForGeolocationMethod(geolocationMethodNameAsString, expectedPositionUnavailableErrorMessage, callbackFunction)
{
    var result = "<!DOCTYPE html>"
    result += "<html>";
    result += "<head>";
    result += "<script>";
    result += checkThatPositionErrorCallbackIsCalledWithPositionUnavailableForGeolocationMethod.toString() + ";";
    result += "checkThatPositionErrorCallbackIsCalledWithPositionUnavailableForGeolocationMethod(" + JSON.stringify(geolocationMethodNameAsString) + ", " + JSON.stringify(expectedPositionUnavailableErrorMessage) + ", " + callbackFunction.toString() + ");";
    result += "</" + "script>";
    result += "</head>";
    result += "<body>";
    result += '<pre id="console"></pre>';
    result += "</body>";
    result += "</html>";
    return result;
}

function dataURLToCheckThatPositionErrorCallbackIsCalledWithPositionUnavailableForGeolocationMethod(geolocationMethodNameAsString, expectedPositionUnavailableErrorMessage, callbackFunction)
{
    return "data:text/html," + encodeURIComponent(markupToCheckThatPositionErrorCallbackIsCalledWithPositionUnavailableForGeolocationMethod(geolocationMethodNameAsString, expectedPositionUnavailableErrorMessage, callbackFunction));
}
