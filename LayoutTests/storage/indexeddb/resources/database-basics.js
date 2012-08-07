if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test the basics of IndexedDB's IDBDatabase.");

function test()
{
    removeVendorPrefixes();

    request = evalAndLog("indexedDB.open('database-basics')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    self.db = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = function() {
        deleteAllObjectStores(db);

        var transaction = event.target.result;
        transaction.oncomplete = setVersionSuccess;
        transaction.onabort = unexpectedErrorCallback;
    };
    request.onerror = unexpectedErrorCallback;
}

function setVersionSuccess()
{
    debug("setVersionSuccess():");

    debug("Testing setVersion.");
    evalAndLog('request = db.setVersion("version a")');
    request.onsuccess = function(event) {

        evalAndExpectException('db.setVersion("version b")',
                               "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        var transaction = event.target.result;
        transaction.oncomplete = setVersionAgain;
        transaction.onabort = unexpectedErrorCallback;
    };
    request.onerror = unexpectedErrorCallback;
}

function setVersionAgain()
{
    request = evalAndLog('db.setVersion("version b")');
    request.onsuccess = createObjectStore;
    request.onerror = unexpectedErrorCallback;
}

function createObjectStore()
{
    shouldBeEqualToString("db.version", "version b");
    shouldBeEqualToString("db.name", "database-basics");
    shouldBe("db.objectStoreNames", "[]");
    shouldBe("db.objectStoreNames.length", "0");
    shouldBe("db.objectStoreNames.contains('')", "false");
    shouldBeUndefined("db.objectStoreNames[0]");
    shouldBeNull("db.objectStoreNames.item(0)");

    objectStore = evalAndLog('db.createObjectStore("test123")');
    checkObjectStore();

    var transaction = event.target.result;
    transaction.oncomplete = testSetVersionAbort;
    transaction.onabort = unexpectedAbortCallback;
}

function checkObjectStore()
{
    shouldBe("db.objectStoreNames", "['test123']");
    shouldBe("db.objectStoreNames.length", "1");
    shouldBe("db.objectStoreNames.contains('')", "false");
    shouldBe("db.objectStoreNames.contains('test456')", "false");
    shouldBe("db.objectStoreNames.contains('test123')", "true");
}


function testSetVersionAbort()
{
    request = evalAndLog('db.setVersion("version c")');
    request.onsuccess = createAnotherObjectStore;
    request.onerror = unexpectedErrorCallback;
}

function createAnotherObjectStore()
{
    shouldBeEqualToString("db.version", "version c");
    shouldBeEqualToString("db.name", "database-basics");
    checkObjectStore();

    objectStore = evalAndLog('db.createObjectStore("test456")');
    var setVersionTrans = evalAndLog("setVersionTrans = event.target.result");
    shouldBeNonNull("setVersionTrans");
    setVersionTrans.oncomplete = unexpectedCompleteCallback;
    setVersionTrans.onabort = checkMetadata;
    setVersionTrans.abort();
}

function checkMetadata()
{
    shouldBeEqualToString("db.version", "version b");
    checkObjectStore();
    testClose();
}

function testClose()
{
    evalAndLog("db.close()");
    debug("Now that the connection is closed, transaction creation should fail");
    evalAndExpectException("db.transaction('test123')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    debug("Call twice, make sure it's harmless");
    evalAndLog("db.close()");
    finishJSTest();
}

test();
