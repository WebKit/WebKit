if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's createObjectStore's various options");

function test()
{
    removeVendorPrefixes();

    request = evalAndLog("indexedDB.open('create-object-store-options')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("request = db.setVersion('version 1')");
    request.onsuccess = cleanDatabase;
    request.onerror = unexpectedErrorCallback;
}

function cleanDatabase()
{
    deleteAllObjectStores(db);

    evalAndLog("db.createObjectStore('a', {keyPath: 'a'})");
    evalAndLog("db.createObjectStore('b')");

    debug("db.createObjectStore('c', {autoIncrement: true});");
    db.createObjectStore('c', {autoIncrement: true});
    event.target.result.oncomplete = setVersionComplete;
}

function setVersionComplete()
{
    trans = evalAndLog("trans = db.transaction(['a', 'b'], IDBTransaction.READ_WRITE)");
    shouldBe("trans.mode", "IDBTransaction.READ_WRITE");

    req = evalAndLog("trans.objectStore('a').put({'a': 0})");
    req.onsuccess = putB;
    req.onerror = unexpectedErrorCallback;

    evalAndExpectExceptionClass("db.createObjectStore('d', 'bar');", "TypeError");
    evalAndExpectExceptionClass("db.createObjectStore('e', false);", "TypeError");
}

function putB()
{
    req = evalAndLog("trans.objectStore('b').put({'a': 0}, 0)");  // OOPS
    req.onsuccess = getA;
    req.onerror = unexpectedErrorCallback;
}

function getA()
{
    req = evalAndLog("trans.objectStore('a').get(0)");
    req.onsuccess = getB;
    req.onerror = unexpectedErrorCallback;
}

function getB()
{
    shouldBe("event.target.result.a", "{a: 0}");

    req = evalAndLog("trans.objectStore('b').get(0)");
    req.onsuccess = checkB;
    req.onerror = unexpectedErrorCallback;
}

function checkB()
{
    shouldBe("event.target.result.a", "{a: 0}");

    finishJSTest();
}

test();
