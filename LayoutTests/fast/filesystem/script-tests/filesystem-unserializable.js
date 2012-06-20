description("This tests if the filesystem object is unserializable by default");

function errorCallback(error)
{
    testFailed("Error occurred: " + error.code);
    finishJSTest();
}

function fileSystemCallback(fs)
{
    try {
        postMessage(fs, '*');
    } catch (e) {
        debug(e.message);
    }

    finishJSTest();
}

function onmessage()
{
}

if (window.webkitRequestFileSystem) {
    window.addEventListener('message', onmessage, false);
    window.jsTestIsAsync = true;
    webkitRequestFileSystem(window.TEMPORARY, 100, fileSystemCallback, errorCallback);
} else
    debug("This test requires FileSystem API support.");
