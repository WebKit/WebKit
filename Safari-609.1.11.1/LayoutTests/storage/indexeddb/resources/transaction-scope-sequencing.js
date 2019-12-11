if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Check that scope restrictions on read-write transactions are enforced.");

indexedDBTest(prepareDatabase, runTransactions);

function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("db.createObjectStore('a')");
    evalAndLog("db.createObjectStore('b')");
    evalAndLog("db.createObjectStore('c')");
}

function runTransactions(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    debug("");

    evalAndLog("transaction1 = db.transaction(['a'], 'readwrite')");
    evalAndLog("transaction1Started = false");
    evalAndLog("transaction1Complete = false");
    request = evalAndLog("transaction1.objectStore('a').get(0)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        debug("");
        evalAndLog("transaction1Started = true");
    };
    transaction1.onabort = unexpectedAbortCallback;
    transaction1.oncomplete = function() {
        debug("");
        evalAndLog("transaction1Complete = true");
        shouldBeFalse("transaction2Started");
        shouldBeFalse("transaction3Started");
    };

    debug("");
    debug("transaction2 overlaps with transaction1, so must wait until transaction1 completes");
    evalAndLog("transaction2 = db.transaction(['a', 'b'], 'readwrite')");
    evalAndLog("transaction2Started = false");
    evalAndLog("transaction2Complete = false");
    request = evalAndLog("transaction2.objectStore('a').get(0)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        debug("");
        shouldBeTrue("transaction1Complete");
        evalAndLog("transaction2Started = true");
    };
    transaction2.onabort = unexpectedAbortCallback;
    transaction2.oncomplete = function() {
        debug("");
        evalAndLog("transaction2Complete = true");
        shouldBeFalse("transaction3Started");
    };

    debug("");
    debug("transaction3 overlaps with transaction2, so must wait until transaction2 completes");
    debug("even though it does not overlap with transaction1");
    evalAndLog("transaction3 = db.transaction(['b', 'c'], 'readwrite')");
    evalAndLog("transaction3Started = false");
    evalAndLog("transaction3Complete = false");
    request = evalAndLog("transaction3.objectStore('b').get(0)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        debug("");
        shouldBeTrue("transaction1Complete");
        shouldBeTrue("transaction2Complete");
        evalAndLog("transaction3Started = true");
    };
    transaction3.onabort = unexpectedAbortCallback;
    transaction3.oncomplete = function() {
        debug("");
        evalAndLog("transaction3Complete = true");
        finishJSTest();
    };
}
