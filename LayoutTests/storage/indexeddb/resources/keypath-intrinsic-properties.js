if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB keyPath with intrinsic properties");

function test()
{
    removeVendorPrefixes();

    request = evalAndLog("indexedDB.deleteDatabase('keypath-intrinsic-properties')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        request = evalAndLog("indexedDB.open('keypath-intrinsic-properties')");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = openSuccess;
    };
}

function openSuccess()
{
    debug("");
    debug("openSuccess():");
    db = evalAndLog("db = event.target.result");
    request = evalAndLog("request = db.setVersion('1')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        transaction = request.result;
        transaction.onabort = unexpectedAbortCallback;
        evalAndLog("store = db.createObjectStore('store', {keyPath: 'id'})");
        evalAndLog("store.createIndex('string length', 'string.length')");
        evalAndLog("store.createIndex('array length', 'array.length')");

        transaction.oncomplete = testKeyPaths;
    };
}

function testKeyPaths()
{
    debug("");
    debug("testKeyPaths():");

    transaction = evalAndLog("transaction = db.transaction('store', IDBTransaction.READ_WRITE)");
    transaction.onabort = unexpectedAbortCallback;
    store = evalAndLog("store = transaction.objectStore('store')");

    for (var i = 0; i < 5; i += 1) {
        var datum = {
            id: 'id#' + i,
            string: Array(i * 2 + 1).join('x'),
            array: Array(i * 3 + 1).join('x').split(/(?:)/)
        };
        evalAndLog("store.put("+JSON.stringify(datum)+")");
    }

    checkStringLengths();

    function checkStringLengths() {
        evalAndLog("request = store.index('string length').openCursor()");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function (e) {
            cursor = e.target.result;
            if (cursor) {
                shouldBe("cursor.key", "cursor.value.string.length");
                cursor.continue();
            } else {
                checkArrayLengths();
            }
        }
    }

    function checkArrayLengths() {
        evalAndLog("request = store.index('array length').openCursor()");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function (e) {
            cursor = e.target.result;
            if (cursor) {
                shouldBe("cursor.key", "cursor.value.array.length");
                cursor.continue();
            }
        }
    }

    transaction.oncomplete = finishJSTest;
}

test();
