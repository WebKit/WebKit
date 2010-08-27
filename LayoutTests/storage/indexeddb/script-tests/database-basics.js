description("Test the basics of IndexedDB's IDBDatabase.");
if (window.layoutTestController) 
    layoutTestController.waitUntilDone();

function openSuccess()
{
    verifySuccessEvent(event);

    var db = evalAndLog("db = event.result");
    shouldBeEqualToString("db.name", "name");
    shouldBe("db.objectStores", "[]");
    shouldBe("db.objectStores.length", "0");
    shouldBe("db.objectStores.contains('')", "false");
    // FIXME: Test .item() once it's possible to get back a non-empty list.

    // FIXME: Test the other properties of IDBDatabase as they're written.

    debug("");
    debug("Testing setVersion.");
    result = evalAndLog('db.setVersion("version a")');
    verifyResult(result);
    result.onsuccess = setVersionAgain;
    result.onError = unexpectedErrorCallback;
}

function setVersionAgain()
{
    result = evalAndLog('db.setVersion("version b")');
    verifyResult(result);
    result.onsuccess = checkVersion;
    result.onError = unexpectedErrorCallback;
}

function checkVersion()
{
    shouldBeEqualToString("db.version", "version b");

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
