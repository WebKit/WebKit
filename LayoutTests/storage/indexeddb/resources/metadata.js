if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB database metadata mutation/snapshotting");

function test()
{
    removeVendorPrefixes();
    evalAndLog("dbname = self.location.pathname");
    evalAndLog("request = indexedDB.deleteDatabase(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = firstOpen;
}

function firstOpen()
{
    debug("");
    debug("firstOpen():");
    evalAndLog("request = indexedDB.open(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("connection1 = request.result");
        evalAndLog("request = connection1.setVersion('1')");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            evalAndLog("trans = request.result");
            evalAndLog("connection1store1 = connection1.createObjectStore('store1')");
            evalAndLog("connection1store1.createIndex('index1', 'path')");

            shouldBeEqualToString("connection1.version", "1");
            shouldBe("connection1.objectStoreNames.length", "1");
            shouldBe("connection1store1.indexNames.length", "1");

            trans.onabort = unexpectedAbortCallback;
            trans.oncomplete = function() {
                debug("Connection's properties should be snapshotted on close");
                evalAndLog("connection1.close()");
                secondOpen();
            };
        };
    };
}

function secondOpen()
{
    debug("");
    debug("secondOpen():");
    evalAndLog("request = indexedDB.open(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("connection2 = request.result");
        evalAndLog("request = connection2.setVersion('2')");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            evalAndLog("trans = request.result");
            evalAndLog("connection2.createObjectStore('store2')");
            evalAndLog("connection2store1 = trans.objectStore('store1')");
            evalAndLog("connection2store1.createIndex('index2', 'path')");

            shouldBeEqualToString("connection2.version", "2");
            shouldBe("connection2.objectStoreNames.length", "2");
            shouldBe("connection2store1.indexNames.length", "2");

            trans.onabort = unexpectedAbortCallback;
            trans.oncomplete = function() {
                debug("Connection's properties should be snapshotted on close");
                evalAndLog("connection2.close()");
                thirdOpen();
            };
        };
    };
}

function thirdOpen()
{
    debug("");
    debug("thirdOpen():");
    evalAndLog("request = indexedDB.open(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("connection3 = request.result");
        evalAndLog("request = connection3.setVersion('3')");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            evalAndLog("trans = request.result");
            evalAndLog("connection3.createObjectStore('store3')");
            evalAndLog("connection3store1 = trans.objectStore('store1')");
            evalAndLog("connection3store1.createIndex('index3', 'path')");

            shouldBeEqualToString("connection3.version", "3");
            shouldBe("connection3.objectStoreNames.length", "3");
            shouldBe("connection3store1.indexNames.length", "3");

            trans.oncomplete = unexpectedCompleteCallback;
            trans.onabort = function() {
                debug("Connection's properties should be snapshotted on close");
                evalAndLog("connection3.close()");
                fourthOpen();
            };
            debug("Connection's properties should be reverted on abort");
            evalAndLog("trans.abort()");
        };
    };
}

function fourthOpen()
{
    debug("");
    debug("fourthOpen():");
    evalAndLog("request = indexedDB.open(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("connection4 = request.result");
        evalAndLog("request = connection4.setVersion('4')");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            evalAndLog("trans = request.result");
            evalAndLog("connection4.createObjectStore('store4')");
            evalAndLog("connection4store1 = trans.objectStore('store1')");
            evalAndLog("connection4store1.createIndex('index4', 'path')");

            shouldBeEqualToString("connection4.version", "4");
            shouldBe("connection4.objectStoreNames.length", "3");
            shouldBe("connection4store1.indexNames.length", "3");

            trans.onabort = unexpectedAbortCallback;
            trans.oncomplete = function() {
                debug("Connection's properties should be snapshotted on close");
                evalAndLog("connection4.close()");
                checkState();
            };
        };
    };
}

function checkState()
{
    debug("");
    debug("checkState():");

    shouldBeEqualToString("connection1.version", "1");
    shouldBe("connection1.objectStoreNames.length", "1");
    shouldBe("connection1store1.indexNames.length", "1");
    debug("");

    shouldBeEqualToString("connection2.version", "2");
    shouldBe("connection2.objectStoreNames.length", "2");
    shouldBe("connection2store1.indexNames.length", "2");
    debug("");

    shouldBeEqualToString("connection3.version", "2");
    shouldBe("connection3.objectStoreNames.length", "2");
    shouldBe("connection3store1.indexNames.length", "2");
    debug("");

    shouldBeEqualToString("connection4.version", "4");
    shouldBe("connection4.objectStoreNames.length", "3");
    shouldBe("connection4store1.indexNames.length", "3");
    debug("");

    finishJSTest();
}

test();
