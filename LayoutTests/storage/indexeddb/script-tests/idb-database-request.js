description("Test IndexedDB's IDBDatabaseRequest.");
if (window.layoutTestController) 
    layoutTestController.waitUntilDone();

function openSuccess()
{
    verifySuccessEvent(event);

    var db = evalAndLog("db = event.result");
    shouldBeEqualToString("db.name", "name");
    shouldBeEqualToString("db.description", "description");
    shouldBeEqualToString("db.version", "");
    shouldBe("db.objectStores", "[]");
    shouldBe("db.objectStores.length", "0");
    shouldBe("db.objectStores.contains('')", "false");
    // FIXME: Test .item() once it's possible to get back a non-empty list.

    // FIXME: Test the other properties of IDBDatabase as they're written.

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
