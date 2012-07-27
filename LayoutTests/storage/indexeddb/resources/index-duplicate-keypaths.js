if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test the use of identical keypaths between objectstores and indexes");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('store-index-collision')");
    request.onsuccess = testCollideStoreIndexSetup;
    request.onerror = unexpectedErrorCallback;
}

function testCollideStoreIndexSetup(evt)
{
    event = evt;
    evalAndLog("db = event.target.result");
    request = evalAndLog("db.setVersion('1')");
    request.onsuccess = testCollideStoreIndex;
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
}

function testCollideStoreIndex(event)
{
    deleteAllObjectStores(db);
    var trans = event.target.result;
    evalAndLog("store = db.createObjectStore('collideWithIndex', {keyPath: 'foo'})");
    evalAndLog("index = store.createIndex('foo', 'foo')");

    trans.oncomplete = storeCollidedStoreIndexData;
    trans.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;
}

function resultShouldBe(v) {
    return function(event) {
        result = event.target.result;
        shouldBeEqualToString("JSON.stringify(result)", v);
    }
};

function storeCollidedStoreIndexData() {
    var trans = db.transaction('collideWithIndex', 'readwrite');

    objectStore = trans.objectStore('collideWithIndex');
    index = objectStore.index('foo');
    evalAndLog("objectStore.put({foo: 10})").onsuccess = function() {
        evalAndLog("objectStore.get(10)").onsuccess = resultShouldBe('{"foo":10}');
        evalAndLog("index.get(10)").onsuccess = resultShouldBe('{"foo":10}');
    };

    trans.oncomplete = testCollideAutoIncrementSetup;
    trans.onabort = unexpectedAbortCallback;
}

function testCollideAutoIncrementSetup()
{
    request = evalAndLog("db.setVersion('2')");
    request.onsuccess = testCollideAutoIncrement;
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
}

function testCollideAutoIncrement()
{
    deleteAllObjectStores(db);
    var trans = request.result;
    evalAndLog("store = db.createObjectStore('collideWithAutoIncrement', {keyPath: 'foo', autoIncrement: true})");
    evalAndLog("index = store.createIndex('foo', 'foo')");

    trans.oncomplete = storeCollidedAutoIncrementData;
    trans.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;
}

function storeCollidedAutoIncrementData()
{
    var trans = db.transaction('collideWithAutoIncrement', 'readwrite');

    objectStore = trans.objectStore('collideWithAutoIncrement');
    index = objectStore.index('foo');
    // Insert some data to futz with the autoIncrement state.
    for (var i = 5; i < 10; i++) {
        evalAndLog("objectStore.put({foo:" +  i + "})");
    }
    // Without a value, this requires the backend to generate a key, which must also be indexed.
    evalAndLog("objectStore.put({'bar': 'baz'})").onsuccess = function(evt) {
        event = evt;
        shouldBe("event.target.result", "10");
        evalAndLog("objectStore.get(10)").onsuccess = resultShouldBe('{"bar":"baz","foo":10}');
        evalAndLog("index.get(10)").onsuccess = resultShouldBe('{"bar":"baz","foo":10}');
    };

    trans.oncomplete = testCollideIndexIndexSetup;
    trans.onabort = unexpectedAbortCallback;
}

function testCollideIndexIndexSetup() {
    finishJSTest();
}

var jsTestIsAsync = true;
test();
