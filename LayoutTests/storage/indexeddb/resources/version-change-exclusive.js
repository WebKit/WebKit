if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Ensure VERSION_CHANGE transaction doesn't run concurrently with other transactions");

function test()
{
    removeVendorPrefixes();
    openDBConnection();
}

function openDBConnection()
{
    evalAndLog("self.state = 'starting'");
    request = evalAndLog("indexedDB.open('version-change-exclusive')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    self.db = evalAndLog("db = event.target.result");

    debug("calling setVersion() - callback should run immediately");
    var versionChangeRequest = evalAndLog("db.setVersion('version 1')");
    versionChangeRequest.onerror = unexpectedErrorCallback;
    versionChangeRequest.onsuccess = inSetVersion;

    // and concurrently...

    debug("calling open() - callback should wait until VERSION_CHANGE transaction is complete");
    var openRequest = evalAndLog("indexedDB.open('version-change-exclusive')");
    openRequest.onsuccess = openAgainSuccess;
    openRequest.onerror = unexpectedErrorCallback;
}

function inSetVersion()
{
    debug("setVersion() callback");
    debug("starting work in VERSION_CHANGE transaction");
    evalAndLog("self.state = 'VERSION_CHANGE started'");

    self.store = evalAndLog("store = db.createObjectStore('test-store')");
    evalAndExpectException("db.transaction('test-store')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    self.count = 0;
    do_async_puts();

    function do_async_puts()
    {
        var req = evalAndLog("store.put(" + count + ", " + count + ")");
        req.onerror = unexpectedErrorCallback;
        req.onsuccess = function (e) {
            debug("in put's onsuccess");
            if (++self.count < 10) {
                do_async_puts();
            } else {
                debug("ending work in VERSION_CHANGE transaction");
                evalAndLog("self.state = 'VERSION_CHANGE finished'");
            }
        };
    }
}

function openAgainSuccess()
{
    debug("open() callback - this should appear after VERSION_CHANGE transaction ends");
    shouldBeEqualToString("self.state", "VERSION_CHANGE finished");
    finishJSTest();
}

test();