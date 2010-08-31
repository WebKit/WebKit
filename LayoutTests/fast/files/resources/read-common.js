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

// Reads a blob either asynchronously or synchronously.
function readBlobAsBinaryString(testFiles, blob)
{
    if (isReadAsAsync())
        _readBlobAsBinaryStringAsync(testFiles, blob);
    else
        _readBlobAsBinaryStringSync(testFiles, blob);
}

function readBlobAsText(testFiles, blob, encoding)
{
    if (isReadAsAsync())
        _readBlobAsTextAsync(testFiles, blob, encoding);
    else
        _readBlobAsTextSync(testFiles, blob, encoding);
}

function readBlobAsDataURL(testFiles, blob)
{
    if (isReadAsAsync())
        _readBlobAsDataURLAsync(testFiles, blob);
    else
        _readBlobAsDataURLSync(testFiles, blob);
}

// Reads a blob asynchronously.
function _readBlobAsBinaryStringAsync(testFiles, blob)
{
    var reader = createReaderAsync(testFiles);
    reader.readAsBinaryString(blob)
}

function _readBlobAsTextAsync(testFiles, blob, encoding)
{
    var reader = createReaderAsync(testFiles);
    reader.readAsText(blob, encoding)
}

function _readBlobAsDataURLAsync(testFiles, blob)
{
    var reader = createReaderAsync(testFiles);
    reader.readAsDataURL(blob)
}

// Reads a blob synchronously.
function _readBlobAsBinaryStringSync(testFiles, blob)
{
    var reader = createReaderSync();
    try {
        var result = reader.readAsBinaryString(blob);
        log(result);
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }

    runNextTest(testFiles);
}

function _readBlobAsTextSync(testFiles, blob, encoding)
{
    var reader = createReaderSync();
    try {
        var result = reader.readAsText(blob, encoding);
        log(result);
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }

    runNextTest(testFiles);
}

function _readBlobAsDataURLSync(testFiles, blob)
{
    var reader = createReaderSync();
    try {
        var result = reader.readAsDataURL(blob);
        log(result);
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }

    runNextTest(testFiles);
}

// Creates a reader for asynchronous reading.
function createReaderAsync(testFiles)
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

// Creates a reader for synchronous reading.
function createReaderSync()
{
    return new FileReaderSync();
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
