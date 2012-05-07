if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test exceptions in IDBRequest handlers cause aborts.");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('exception-in-event-aborts')");
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
    evalAndLog("trans.oncomplete = startTest");

    deleteAllObjectStores(db);

    store = evalAndLog("store = db.createObjectStore('storeName', null)");
    request = evalAndLog("store.add({x: 'value', y: 'zzz'}, 'key')");
    request.onerror = unexpectedErrorCallback;
}

function startTest()
{
    debug("");
    trans = evalAndLog("trans = db.transaction(['storeName'], 'readwrite')");
    evalAndLog("trans.onabort = transactionAborted1");
    evalAndLog("trans.oncomplete = unexpectedCompleteCallback");
    store = evalAndLog("store = trans.objectStore('storeName')");
    request = evalAndLog("store.add({x: 'value2', y: 'zzz2'}, 'key2')");
    trans.addEventListener('success', causeException, true);
    request.onerror = unexpectedErrorCallback;
}

function causeException()
{
    debug("");
    evalAndLog("event.preventDefault()");
    debug("Throwing");
    throw "this exception is expected";
}

function transactionAborted1()
{
    debug("");
    shouldHaveHadError("this exception is expected");
    testPassed("The transaction was aborted.");
    trans = evalAndLog("trans = db.transaction(['storeName'], 'readwrite')");
    evalAndLog("trans.onabort = transactionAborted2");
    evalAndLog("trans.oncomplete = unexpectedCompleteCallback");
    store = evalAndLog("store = trans.objectStore('storeName')");
    request = evalAndLog("store.add({x: 'value', y: 'zzz'}, 'key')");
    request.onsuccess = unexpectedSuccessCallback;
    db.onerror = causeException;
}

function transactionAborted2()
{
    debug("");
    shouldHaveHadError("this exception is expected");
    testPassed("The transaction was aborted.");
    trans = evalAndLog("trans = db.transaction(['storeName'], 'readwrite')");
    evalAndLog("trans.onabort = unexpectedAbortCallback");
    evalAndLog("trans.oncomplete = transactionCompleted1");
    store = evalAndLog("store = trans.objectStore('storeName')");
    request = evalAndLog("store.add({x: 'value3', y: 'zzz3'}, 'key3')");
    request.onsuccess = throwAndCatch;
    request.onerror = unexpectedErrorCallback;
    db.onerror = null;
}

function throwAndCatch()
{
    debug("");
    evalAndLog("event.preventDefault()");
    debug("Throwing within a try block");
    try {
        throw "AHHH";
    } catch (e) {
    }
}

function transactionCompleted1()
{
    debug("");
    testPassed("The transaction completed.");
    trans = evalAndLog("trans = db.transaction(['storeName'], 'readwrite')");
    evalAndLog("trans.onabort = unexpectedAbortCallback");
    evalAndLog("trans.oncomplete = transactionCompleted2");
    store = evalAndLog("store = trans.objectStore('storeName')");
    request = evalAndLog("store.add({x: 'value4', y: 'zzz4'}, 'key4')");
    request.onsuccess = function() { testPassed("key4 added"); }
    request.onerror = unexpectedErrorCallback;
}

function transactionCompleted2()
{
    debug("");
    testPassed("The transaction completed.");
    debug("");
    finishJSTest();
}

test();