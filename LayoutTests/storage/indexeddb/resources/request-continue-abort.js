if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Regression test for IDBRequest issue calling continue on a cursor then aborting.");

function test()
{
    removeVendorPrefixes();

    evalAndLog('dbname = self.location.pathname');
    request = evalAndLog("indexedDB.deleteDatabase('dbname')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        request = evalAndLog("request = indexedDB.open('dbname')");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            evalAndLog("db = request.result");
            request = evalAndLog("db.setVersion('1')");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = unexpectedBlockedCallback;
            request.onsuccess = function() {
                var transaction = request.result;
                transaction.onabort = unexpectedAbortCallback;
                onUpgrade();
                transaction.oncomplete = testCursor;
            };
        };
    };
}

function onUpgrade()
{
    debug("");
    debug("onUpgrade:");
    evalAndLog("db.createObjectStore('store')");
}


function testCursor()
{
    debug("");
    debug("testCursor:");
    evalAndLog("transaction = db.transaction('store', 'readwrite')");

    evalAndLog("store = transaction.objectStore('store')");
    request = evalAndLog("store.add('value1', 'key1')");
    request.onerror = unexpectedErrorCallback;
    request = evalAndLog("store.add('value2', 'key2')");
    request.onerror = unexpectedErrorCallback;

    debug("");
    evalAndLog("state = 0");
    evalAndLog("request = store.openCursor()");
    request.onsuccess = function() {
        debug("");
        debug("'success' event fired at request.");
        shouldBe("++state", "1");
        evalAndLog("request.result.continue()");
        transaction.abort();
    };
    request.onerror = function() {
        debug("");
        debug("'error' event fired at request.");
        shouldBe("++state", "2");
    };
    transaction.oncomplete = unexpectedCompleteCallback;
    transaction.onabort = function() {
        debug("");
        debug("'abort' event fired at transaction.");
        shouldBe("++state", "3");
        finishJSTest();
    };
}

test();
