if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB transaction does not crash on abort.");

function test()
{
    removeVendorPrefixes();

    request = evalAndLog("indexedDB.open('transaction-crash-on-abort')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    debug("openSuccess():");
    db = evalAndLog("db = event.target.result");
    request = evalAndLog("db.setVersion('1.0')");
    request.onsuccess = setVersionSuccess;
    request.onerror = unexpectedErrorCallback;
}

function setVersionSuccess()
{
    evalAndLog("db.createObjectStore('foo')");
    event.target.result.oncomplete = setVersionComplete;
}

function setVersionComplete()
{
    evalAndLog("db.transaction('foo')");
    evalAndLog("self.gc()");
    finishJSTest();
}

test();