if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

if (window.testRunner) {
    testRunner.setOriginQuotaRatioEnabled(false);
    testRunner.setAllowStorageQuotaIncrease(false);
}

description('This test verifies that estimated size of IDB database task is not smaller than or close to actual space increase (maybe subject to our implementation of backing store.)');

const quota = 400 * 1024; // Current default quota for test.
const indexCount = 20;
const indexValueSize = 1024;
const keySize = 1024;

indexedDBTest(prepareDatabase, onOpenSuccess);

function randomKey(length) {
    var str = "";
    for (var i = 0; i < length; i++)
        str += (Math.floor(Math.random() * 10))
    return str;
}

function randomPropertyValue(length) {
    var value = [];
    for (var i = 0; i < length; i++)
        value.push(Math.floor(Math.random() * 10))
    return value;
}

function createObject() {
    var obj = { };
    for (var i = 0; i < indexCount; i++) 
        obj["index" + i] = randomPropertyValue(indexValueSize);

    return obj;
}

function prepareDatabase(event)
{
    preamble(event);
    db = event.target.result;
    store = db.createObjectStore('store');
    var indexes = [];

    debug("Create " + indexCount + " indexes");
    // This is an extreme case where each of indexes is composed of all properties of the object.
    for (var i = 0; i < indexCount; i ++)
        indexes.push("index" + i);
    for (var i = 0; i < indexCount; i ++)
        store.createIndex("allIndexes" + i, indexes);
}

function onOpenSuccess(event)
{
    preamble(event);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.transaction('store', 'readwrite').objectStore('store')");
    request = evalAndLog("request = store.add(createObject(), randomKey(keySize))");
    request.onerror = () => {
        shouldBeTrue("'error' in request");
        shouldBe("request.error.code", "DOMException.QUOTA_EXCEEDED_ERR");
        shouldBeEqualToString("request.error.name", "QuotaExceededError");

        finishJSTest();
    }
    request.onsuccess = () => {
        testFailed("Add operation should fail because task size is bigger than space available");
        finishJSTest();
    }
}