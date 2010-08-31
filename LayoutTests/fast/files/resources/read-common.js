// Builds a blob from a list of items.
// If the 'builder' argument is not provided, create a new one.
function buildBlob(items, builder)
{
    if (builder === undefined)
        builder = new BlobBuilder();
    for (var i = 0; i < items.length; i++)
        builder.append(items[i]);
    return builder.getBlob();
}

function readBlobAsBinaryString(testFiles, blob)
{
    var reader = createReader(testFiles);
    reader.readAsBinaryString(blob)
}

function readBlobAsText(testFiles, blob, encoding)
{
    var reader = createReader(testFiles);
    reader.readAsText(blob, encoding)
}

function readBlobAsDataURL(testFiles, blob)
{
    var reader = createReader(testFiles);
    reader.readAsDataURL(blob)
}

function createReader(testFiles)
{
    var reader = new FileReader();

    reader.onloadstart = loadStarted;
    reader.onload = loaded;
    reader.onabort = logEvent;
    reader.onloadend = function(event) { loadEnded(testFiles, event); }
    reader.onerror = loadFailed;

    log("readyState: " + reader.readyState);
    return reader;
}

function logEvent(event)
{
    log("Received " + event.type + " event");
}

function loadStarted(event)
{
    logEvent(event);
    log("readyState: " + event.target.readyState);
}

function loaded(event)
{
    logEvent(event);
    log("readyState: " + event.target.readyState);
    log("result size: " + event.target.result.length);

    var result = event.target.result;
    var resultOutput = _isASCIIString(result) ? result : _toHexadecimal(result);
    log("result: " + resultOutput);
}

function loadFailed(event)
{
    logEvent(event);
    log("readyState: " + event.target.readyState);
    log("error code: " + event.target.error.code);
}

function loadEnded(testFiles, event)
{
    logEvent(event);
    runNextTest(testFiles);
}

// Helper functions.
function _isASCIIString(str)
{
    for (var i = 0; i < str.length; ++i) {
        if (str.charCodeAt(i) >= 128)
            return false;
    }
    return true;
}

function _toHexadecimal(str)
{
    var result = "";
    for (var i = 0; i < str.length; ++i) {
        var hex = "0x" + (str.charCodeAt(i) & 0xFF).toString(16);
        if (i > 0)
            result += " ";
        result += hex;
    }
    return result;
}
