if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB undefined as record value");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");
    result = evalAndLog("result = objectStore.add(undefined, Infinity);");
    result.onerror = unexpectedErrorCallback;
    result.onsuccess = getValue;
}

function getValue()
{
    result = evalAndLog("result = objectStore.get(Infinity);");
    result.onerror = unexpectedErrorCallback;
    result.onsuccess = checkValue;
}

function checkValue()
{
    value = evalAndLog("value = event.target.result;");
    shouldBe("value", "undefined");
    result = evalAndLog("result = objectStore.openCursor();");
    result.onerror = unexpectedErrorCallback;
    result.onsuccess = checkCursor;
}

function checkCursor()
{
    cursor = evalAndLog("cursor = event.target.result;");
    if (cursor) {
        shouldBe("cursor.key", "Infinity");
        shouldBe("cursor.value", "undefined");
    } else {
        testFailed("cursor is null");
    }
    finishJSTest();
}
