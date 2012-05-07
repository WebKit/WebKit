if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test features of IndexedDB's multiEntry indices.");

function test()
{
    removeVendorPrefixes();

    request = evalAndLog("indexedDB.open('index-multiEntry')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        db = evalAndLog("db = event.target.result");

        request = evalAndLog("db.setVersion('1')");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = prepareDatabase;
    };
}

function prepareDatabase()
{
    debug("");
    debug("Creating empty stores and indexes");
    var trans = evalAndLog("trans = event.target.result");
    shouldBeTrue("trans !== null");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = addData;

    deleteAllObjectStores(db);

    store = evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("store.createIndex('index', 'x', {multiEntry: true})");

    store2 = evalAndLog("store2 = db.createObjectStore('store-unique')");
    evalAndLog("store2.createIndex('index-unique', 'x', {multiEntry: true, unique: true})");
}

function addData()
{
    debug("");
    debug("Populating stores (and indexes)");
    transaction = evalAndLog("transaction = db.transaction(['store'], 'readwrite')");
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = function() { verifyIndexes('index', verifyUniqueConstraint); };

    debug("First try some keys that aren't what we're expecting");
    request = evalAndLog("transaction.objectStore('store').put({x: [7, 8, 9], y: 'a'}, 'foo')");
    request.onerror = unexpectedErrorCallback;
    debug("Now overwrite them with what we're expecting");
    request = evalAndLog("transaction.objectStore('store').put({x: [1, 2, 3], y: 'a'}, 'foo')");
    request.onerror = unexpectedErrorCallback;
    request = evalAndLog("transaction.objectStore('store').put({x: [4, 5, 6], y: 'b'}, 'bar')");
    request.onerror = unexpectedErrorCallback;
}

function verifyIndexes(indexName, callback)
{
    debug("");
    debug("Verifying index: " + indexName);
    transaction = evalAndLog("transaction = db.transaction(['store'], 'readonly')");
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = callback;

    expected = [
        { key: 1, primaryKey: 'foo', y: 'a' },
        { key: 2, primaryKey: 'foo', y: 'a' },
        { key: 3, primaryKey: 'foo', y: 'a' },
        { key: 4, primaryKey: 'bar', y: 'b' },
        { key: 5, primaryKey: 'bar', y: 'b' },
        { key: 6, primaryKey: 'bar', y: 'b' },
    ];

    var request = evalAndLog("transaction.objectStore('store').index('" + indexName + "').openCursor()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(event) {
        cursor = evalAndLog("cursor = event.target.result");
        if (cursor) {
            ex = expected.shift();
            shouldBeTrue("ex != null");
            shouldBe("cursor.key", String(ex.key));
            shouldBeEqualToString("cursor.primaryKey", ex.primaryKey);
            shouldBeEqualToString("cursor.value.y", ex.y);
            cursor.continue();
        } else {
            shouldBeTrue("expected.length === 0");
        }
    };
}

function verifyUniqueConstraint()
{
    debug("");
    debug("Verifying unique constraint on multiEntry index");
    transaction = evalAndLog("transaction = db.transaction(['store-unique'], 'readwrite')");
    transaction.onabort = function () {
        debug("Transaction aborted as expected");
        createIndexOnStoreWithData();
    };
    transaction.oncomplete = unexpectedCompleteCallback;

    request = evalAndLog("transaction.objectStore('store-unique').put({x: [1, 2, 3], y: 'a'}, 'foo')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        debug("success!");
        debug("Replace an existing record - this should work");
        request = evalAndLog("transaction.objectStore('store-unique').put({x: [1, 2, 7], y: 'a'}, 'foo')");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            debug("success!");
            debug("This should fail the uniqueness constraint on the index, and fail:");
            request = evalAndLog("transaction.objectStore('store-unique').put({x: [5, 2], y: 'c'}, 'should fail')");
            request.onsuccess = unexpectedSuccessCallback;
            request.onerror = function() { debug("Request failed, as expected"); };
        };
    };
}

function createIndexOnStoreWithData()
{
    debug("");
    debug("Create an index on a populated store");

    request = evalAndLog("db.setVersion('2')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {

        var trans = evalAndLog("trans = event.target.result");
        shouldBeTrue("trans !== null");
        trans.onabort = unexpectedAbortCallback;
        trans.oncomplete = function() { verifyIndexes('index-new', finishJSTest); };

        store = evalAndLog("store = trans.objectStore('store')");
        evalAndLog("store.createIndex('index-new', 'x', {multiEntry: true})");
    };
}

test();