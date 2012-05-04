if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Tests IndexedDB's quota enforcing mechanism.");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('database-quota')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    self.db = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = setVersionSuccess;
    request.onerror = unexpectedErrorCallback;
}

function setVersionSuccess()
{
    debug("setVersionSuccess():");
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeTrue("trans !== null");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = checkQuotaEnforcing;

    deleteAllObjectStores(db);

    shouldBeEqualToString("db.version", "new version");
    shouldBeEqualToString("db.name", "database-quota");
    shouldBe("db.objectStoreNames", "[]");
    shouldBe("db.objectStoreNames.length", "0");
    shouldBe("db.objectStoreNames.contains('')", "false");

    objectStore = evalAndLog('db.createObjectStore("test123")');
    checkObjectStore();
}

function checkObjectStore()
{
    shouldBe("db.objectStoreNames", "['test123']");
    shouldBe("db.objectStoreNames.length", "1");
    shouldBe("db.objectStoreNames.contains('')", "false");
    shouldBe("db.objectStoreNames.contains('test456')", "false");
    shouldBe("db.objectStoreNames.contains('test123')", "true");
}

function checkQuotaEnforcing()
{
    var trans = evalAndLog("trans = db.transaction(['test123'], IDBTransaction.READ_WRITE)");
    trans.onabort = testComplete;
    trans.oncomplete = unexpectedCompleteCallback;
    debug("Creating 'data' which contains 64K of data");
    self.data = "X";
    for (var i = 0; i < 16; i++)
        data += data;
    shouldBe("data.length", "65536");
    self.dataAdded = 0;
    self.store = evalAndLog("store = trans.objectStore('test123')");
    addData();
}

function addData()
{
    if (dataAdded < 5 * 1024 * 1024) {
        if (dataAdded > 0)
            store = event.target.source;
    } else {
        testFailed("added more than quota");
        finishJSTest();
        return;
    }
    dataAdded += 65536;
    request = store.add({x: data}, dataAdded);
    request.onsuccess = addData;
    request.onerror = logError;
}

function logError()
{
    debug("Error function called: (" + event.target.errorCode + ") " + event.target.webkitErrorMessage);
    evalAndLog("event.preventDefault()");
}

function testComplete()
{
    testPassed("Adding data failed due to quota error. Data added was about " + Math.round(dataAdded / 1024 / 1024) + " MB");
    finishJSTest();
}

test();