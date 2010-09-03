description("Test the basics of IndexedDB's IDBDatabase.");
if (window.layoutTestController) 
    layoutTestController.waitUntilDone();

function openSuccess()
{
    verifySuccessEvent(event);

    var db = evalAndLog("db = event.result");
    deleteAllObjectStores(db);

    // We must do something asynchronous before anything synchronous since
    // deleteAllObjectStores only schedules the object stores to be removed.
    // We don't know for sure whether it's happened until an IDBRequest object
    // that was created after the removes fires.

    debug("Testing setVersion.");
    result = evalAndLog('db.setVersion("version a")');
    verifyResult(result);
    result.onsuccess = setVersionAgain;
    result.onError = unexpectedErrorCallback;
}

function setVersionAgain()
{
    verifySuccessEvent(event);

    result = evalAndLog('db.setVersion("version b")');
    verifyResult(result);
    result.onsuccess = createObjectStore;
    result.onError = unexpectedErrorCallback;
}

function createObjectStore()
{
    verifySuccessEvent(event);
    shouldBeEqualToString("db.version", "version b");
    shouldBeEqualToString("db.name", "name");
    shouldBe("db.objectStores", "[]");
    shouldBe("db.objectStores.length", "0");
    shouldBe("db.objectStores.contains('')", "false");

    result = evalAndLog('db.createObjectStore("test123")');
    verifyResult(result);
    result.onsuccess = checkObjectStore;
    result.onError = unexpectedErrorCallback;
}

function checkObjectStore()
{
    verifySuccessEvent(event);
    shouldBe("db.objectStores", "['test123']");
    shouldBe("db.objectStores.length", "1");
    shouldBe("db.objectStores.contains('')", "false");
    shouldBe("db.objectStores.contains('test456')", "false");
    shouldBe("db.objectStores.contains('test123')", "true");

    done();
}

function test()
{
    result = evalAndLog("indexedDB.open('name', 'description')");
    verifyResult(result);
    result.onsuccess = openSuccess;
    result.onerror = unexpectedErrorCallback;
}

test();

var successfullyParsed = true;
