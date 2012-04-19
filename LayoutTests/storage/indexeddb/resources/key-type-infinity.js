if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB key types");

function test()
{
    removeVendorPrefixes();

    name = self.location.pathname;
    request = evalAndLog("indexedDB.open(name)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("request = db.setVersion('1')");
    request.onsuccess = testGroup1;
    request.onerror = unexpectedErrorCallback;
}

function testGroup1()
{
    deleteAllObjectStores(db);

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

test();