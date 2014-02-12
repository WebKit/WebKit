if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB database metadata mutation/snapshotting");

indexedDBTest(prepareDatabase, snapshotConnection1);
function prepareDatabase()
{
    connection1 = event.target.result;
    evalAndLog("connection1store1 = connection1.createObjectStore('store1')");
    evalAndLog("connection1store1.createIndex('index1', 'path')");

    shouldBe("connection1.version", "1");
    shouldBe("connection1.objectStoreNames.length", "1");
    shouldBe("connection1store1.indexNames.length", "1");
}

function snapshotConnection1()
{
    debug("Connection's properties should be snapshotted on close");
    evalAndLog("connection1.close()");
    secondOpen();
}

function secondOpen()
{
    debug("");
    debug("secondOpen():");
    evalAndLog("request = indexedDB.open(dbname, 2)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = function() {
        evalAndLog("connection2 = request.result");
        evalAndLog("trans = request.transaction");
        evalAndLog("connection2.createObjectStore('store2')");
        evalAndLog("connection2store1 = trans.objectStore('store1')");
        evalAndLog("connection2store1.createIndex('index2', 'path')");

        shouldBe("connection2.version", "2");
        shouldBe("connection2.objectStoreNames.length", "2");
        shouldBe("connection2store1.indexNames.length", "2");
    };
    request.onsuccess = function() {
        debug("Connection's properties should be snapshotted on close");
        evalAndLog("connection2.close()");
        thirdOpen();
    };
}

function thirdOpen()
{
    debug("");
    debug("thirdOpen():");
    evalAndLog("request = indexedDB.open(dbname, 3)");
    request.onsuccess = unexpectedSuccessCallback;
    request.onupgradeneeded = function() {
        evalAndLog("connection3 = request.result");
        evalAndLog("trans = request.transaction");
        evalAndLog("connection3.createObjectStore('store3')");
        evalAndLog("connection3store1 = trans.objectStore('store1')");
        evalAndLog("connection3store1.createIndex('index3', 'path')");

        shouldBe("connection3.version", "3");
        shouldBe("connection3.objectStoreNames.length", "3");
        shouldBe("connection3store1.indexNames.length", "3");

        trans.oncomplete = unexpectedCompleteCallback;
        debug("Connection's properties should be reverted on abort");
        evalAndLog("trans.abort()");
    };
    request.onerror = function() {
        debug("Connection's properties should be snapshotted on close");
        evalAndLog("connection3.close()");
        fourthOpen();
    }
}

function fourthOpen()
{
    debug("");
    debug("fourthOpen():");
    evalAndLog("request = indexedDB.open(dbname, 4)");
    request.onerror = unexpectedErrorCallback;
    request.onupgradeneeded = function() {
        evalAndLog("connection4 = request.result");
        evalAndLog("trans = request.transaction");
        evalAndLog("connection4.createObjectStore('store4')");
        evalAndLog("connection4store1 = trans.objectStore('store1')");
        evalAndLog("connection4store1.createIndex('index4', 'path')");

        shouldBe("connection4.version", "4");
        shouldBe("connection4.objectStoreNames.length", "3");
        shouldBe("connection4store1.indexNames.length", "3");
    };
    request.onsuccess = function() {
        debug("Connection's properties should be snapshotted on close");
        evalAndLog("connection4.close()");
        checkState();
    };
}

function checkState()
{
    debug("");
    debug("checkState():");

    shouldBe("connection1.version", "1");
    shouldBe("connection1.objectStoreNames.length", "1");
    shouldBe("connection1store1.indexNames.length", "1");
    debug("");

    shouldBe("connection2.version", "2");
    shouldBe("connection2.objectStoreNames.length", "2");
    shouldBe("connection2store1.indexNames.length", "2");
    debug("");

    shouldBe("connection3.version", "2");
    shouldBe("connection3.objectStoreNames.length", "2");
    shouldBe("connection3store1.indexNames.length", "2");
    debug("");

    shouldBe("connection4.version", "4");
    shouldBe("connection4.objectStoreNames.length", "3");
    shouldBe("connection4store1.indexNames.length", "3");
    debug("");

    finishJSTest();
}
