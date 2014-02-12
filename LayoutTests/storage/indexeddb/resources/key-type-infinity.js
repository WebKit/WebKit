if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB key types");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    objectStore = evalAndLog("db.createObjectStore('foo');");
    debug("test key as infinity");
    request = evalAndLog("request = objectStore.add([], Infinity);");
    request.onsuccess = testGroup2;
    request.onerror = unexpectedErrorCallback;
}

function testGroup2()
{
    debug("test key as negative infinity");
    request = evalAndLog("request = objectStore.add([], -Infinity);");
    request.onsuccess = testGroup3;
    request.onerror = unexpectedErrorCallback;
}

function testGroup3()
{
    finishJSTest();
}
