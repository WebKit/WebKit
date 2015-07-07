// Builds a blob from a list of items.
// The 'contentType' argument is optional.
// If the 'builder' argument is not provided, create a new one.
function buildBlob(items, contentType)
{
    if (contentType === undefined)
        return new Blob(items);
    return new Blob(items, {type:contentType});
}

// Reads a blob either asynchronously or synchronously.
function readBlobAsArrayBuffer(testFiles, blob)
{
    if (isReadAsAsync())
        _readBlobAsArrayBufferAsync(testFiles, blob);
    else
        _readBlobAsArrayBufferSync(testFiles, blob);
}

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
function _readBlobAsArrayBufferAsync(testFiles, blob)
{
    var reader = createReaderAsync(testFiles);
    reader.readAsArrayBuffer(blob)
}

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
function _readBlobAsArrayBufferSync(testFiles, blob)
{
    var reader = createReaderSync();
    try {
        var result = reader.readAsArrayBuffer(blob);
        logResult(result);
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }

    try {
        var result = reader.readAsArrayBuffer(12345);
        logResult(result);
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }

    runNextTest(testFiles);
}

function _readBlobAsBinaryStringSync(testFiles, blob)
{
    var reader = createReaderSync();
    try {
        var result = reader.readAsBinaryString(blob);
        logResult(result);
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }

    try {
        var result = reader.readAsBinaryString(12345);
        logResult(result);
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
        logResult(result);
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }

    try {
        var result = reader.readAsText(12345, encoding);
        logResult(result);
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
        logResult(result);
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }

    try {
        var result = reader.readAsDataURL(12345);
        logResult(result);
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

// 'result' can be either an ArrayBuffer object or a string.
function logResult(result)
{
    if (result === null) {
        log("result: null");
        return;
    }
    if (typeof result == 'object') {
        log("result size: " + result.byteLength);
        result = new Uint8Array(result, 0, result.byteLength);
    } else
        log("result size: " + result.length);

    var resultOutput = _isASCIIString(result) ? _toString(result) : _toHexadecimal(result);
    log("result: " + resultOutput);
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

    var result = event.target.result;
    logResult(result);
}

function loadFailed(event)
{
    logEvent(event);
    log("readyState: " + event.target.readyState);
    log("error code: " + event.target.error.code);
    logResult(event.target.result);
}

function loadEnded(testFiles, event)
{
    logEvent(event);
    runNextTest(testFiles);
}

// Helper functions.

// 'value' can be either an ArrayBufferView object or a string.
function _toString(value)
{
    if (typeof value == 'string')
        return value;
    var result = "";
    for (var i = 0; i < value.length; ++i)
        result += String.fromCharCode(value[i]);
    return result;
}

// 'value' can be either an ArrayBufferView object or a string.
function _isASCIIString(value)
{
    for (var i = 0; i < value.length; ++i) {
        if (_getElementAt(value, i) >= 128)
            return false;
    }
    return true;
}

// 'value' can be either an ArrayBufferView object or a string.
function _toHexadecimal(value)
{
    var result = "";
    for (var i = 0; i < value.length; ++i) {
        var hex = "0x" + (_getElementAt(value, i) & 0xFF).toString(16);
        if (i > 0)
            result += " ";
        result += hex;
    }
    return result;
}

function _getElementAt(value, i)
{
    return (typeof value == 'string') ? value.charCodeAt(i) : value[i];
}
