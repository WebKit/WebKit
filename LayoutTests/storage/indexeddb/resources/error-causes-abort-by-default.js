if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Verify that a transaction with an error aborts unless preventDefault() is called.");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('error-causes-abort-by-default')");
    request.onsuccess = setVersion;
    request.onerror = unexpectedErrorCallback;
}

function setVersion()
{
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = deleteExisting;
    request.onerror = unexpectedErrorCallback;
}

function deleteExisting()
{
    debug("setVersionSuccess():");
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeTrue("trans !== null");
    trans.onabort = unexpectedAbortCallback;
    evalAndLog("trans.oncomplete = addData");

    deleteAllObjectStores(db);

    evalAndLog("db.createObjectStore('storeName', null)");
}

function addData()
{
    trans = evalAndLog("trans = db.transaction(['storeName'], 'readwrite')");
    evalAndLog("trans.onabort = unexpectedAbortCallback");
    evalAndLog("trans.oncomplete = transactionCompleted");
    store = evalAndLog("store = trans.objectStore('storeName')");
    request = evalAndLog("store.add({x: 'value', y: 'zzz'}, 'key')");
    request.onsuccess = addMore;
    request.onerror = unexpectedErrorCallback;
}

function addMore()
{
    
    request = evalAndLog("event.target.source.add({x: 'value', y: 'zzz'}, 'key')");
    request.onsuccess = unexpectedSuccessCallback;
    request.addEventListener("error", preventTheDefault);
}

function preventTheDefault()
{
    evalAndLog("event.preventDefault()");
}

function transactionCompleted()
{
    testPassed("Transaction completed");
    debug("");
    debug("");
    trans = evalAndLog("trans = db.transaction(['storeName'], 'readwrite')");
    evalAndLog("trans.onabort = transactionAborted1");
    evalAndLog("trans.oncomplete = unexpectedCompleteCallback");
    store = evalAndLog("store = trans.objectStore('storeName')");
    request = evalAndLog("store.add({x: 'value', y: 'zzz'}, 'key')");
    request.onsuccess = unexpectedSuccessCallback;
    request.onerror = allowDefault;
}

function allowDefault()
{
    debug("Doing nothing to prevent the default action...");
}

function transactionAborted1()
{
    testPassed("Transaction aborted");
    debug("");
    debug("");
    trans = evalAndLog("trans = db.transaction(['storeName'], 'readwrite')");
    evalAndLog("trans.onabort = transactionAborted2");
    evalAndLog("trans.oncomplete = unexpectedCompleteCallback");
    store = evalAndLog("store = trans.objectStore('storeName')");
    request = evalAndLog("store.add({x: 'value', y: 'zzz'}, 'key')");
    request.onsuccess = unexpectedSuccessCallback;
    debug("Omitting an onerror handler");
}

function transactionAborted2()
{
    testPassed("Transaction aborted");
    finishJSTest();
}

test();