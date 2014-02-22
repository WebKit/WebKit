// This goes before everything else to keep console message line number invariant.
var lastXHRIndex = 0;
function xhrLoadedCallback()
{
    // We need to make sure the console message text is unique so that we don't end up with repeat count update only.
    console.log("XHR loaded: " + (++lastXHRIndex));
}

var initialize_NetworkTest = function() {

InspectorTest.dumpNetworkRequests = function()
{
    var requests = WebInspector.panel("network").requests.slice();
    requests.sort(function(a, b) {return a.url.localeCompare(b.url);});
    InspectorTest.addResult("resources count = " + requests.length);
    for (i = 0; i < requests.length; i++)
        InspectorTest.addResult(requests[i].url);
}

InspectorTest.resetInspectorResourcesData = function(callback)
{
    InspectorTest.evaluateInPage("resetInspectorResourcesData()", nextStep);

    function nextStep(result)
    {
        if (!result) {
            InspectorTest.addResult("This test can not be run as window.internals is not available.");
            Inspector.completeTest();
        } else
            callback();
    }
}

InspectorTest.makeSimpleXHR = function(method, url, async, callback)
{
    InspectorTest.makeXHR(method, url, async, undefined, undefined, [], false, undefined, callback);
}

InspectorTest.makeSimpleXHRWithPayload = function(method, url, async, payload, callback)
{
    InspectorTest.makeXHR(method, url, async, undefined, undefined, [], false, payload, callback);
}

InspectorTest.makeXHR = function(method, url, async, user, password, headers, withCredentials, payload, callback)
{
    var args = {};
    args.method = method;
    args.url = url;
    args.async = async;
    args.user = user;
    args.password = password;
    args.headers = headers;
    args.withCredentials = withCredentials;
    args.payload = payload;
    var jsonArgs = JSON.stringify(args).replace(/\"/g, "\\\"");

    function innerCallback(msg)
    {
        if (msg._messageText.indexOf("XHR loaded") !== -1)
            callback();
        else
            InspectorTest.addConsoleSniffer(innerCallback);
    }

    InspectorTest.addConsoleSniffer(innerCallback);
    InspectorTest.evaluateInPage("makeXHRForJSONArguments(\"" + jsonArgs + "\")");
}

};

function makeSimpleXHR(method, url, async, callback)
{
    makeSimpleXHRWithPayload(method, url, async, null, callback);
}

function makeSimpleXHRWithPayload(method, url, async, payload, callback)
{
    makeXHR(method, url, async, undefined, undefined, [], false, payload, callback)
}

function makeXHR(method, url, async, user, password, headers, withCredentials, payload, callback)
{
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function()
    {
        if (xhr.readyState === XMLHttpRequest.DONE) {
            if (typeof(callback) === "function")
                callback();
        }
    }
    xhr.open(method, url, async, user, password);
    xhr.withCredentials = withCredentials;
    for (var  i = 0; i < headers.length; ++i)
        xhr.setRequestHeader(headers[i][0], headers[i][1]);
    xhr.send(payload);
}

function makeXHRForJSONArguments(jsonArgs)
{
    var args = JSON.parse(jsonArgs);
    makeXHR(args.method, args.url, args.async, args.user, args.password, args.headers, args.withCredentials, args.payload, xhrLoadedCallback);
}


function resetInspectorResourcesData()
{
    if (!window.internals)
        return false;

    internals.setInspectorResourcesDataSizeLimits(10 * 1000 * 1000, 1000 * 1000);
    return true;
}

