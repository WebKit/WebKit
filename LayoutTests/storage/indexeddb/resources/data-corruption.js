if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that data inserted into IndexedDB does not get corrupted on disk.");

indexedDBTest(prepareDatabase, addData);
function prepareDatabase()
{
    db = event.target.result;
    debug("setVersionSuccess():");
    shouldBeEqualToString("event.dataLoss", "none");
    self.trans = evalAndLog("trans = event.target.transaction");
    shouldBeNonNull("trans");
    trans.onabort = unexpectedAbortCallback;

    evalAndLog("db.createObjectStore('storeName')");
}

var testDate = new Date('February 24, 1955 12:00:08');

function addData()
{
    debug("addData():");
    var transaction = evalAndLog("transaction = db.transaction(['storeName'], 'readwrite')");
    var request = evalAndLog("request = transaction.objectStore('storeName').add({x: testDate}, 'key')");
    request.onerror = unexpectedErrorCallback;
    transaction.oncomplete = getData;
}

function getData()
{
    debug("addData():");
    var transaction = evalAndLog("transaction = db.transaction(['storeName'], 'readonly')");
    var request = evalAndLog("request = transaction.objectStore('storeName').get('key')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = doCheck;
}

function doCheck()
{
    shouldBe("event.target.result.x.toString()", "testDate.toString()");
    finishJSTest();
}
