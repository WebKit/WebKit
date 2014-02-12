if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB keyPath with intrinsic properties");

indexedDBTest(prepareDatabase, testKeyPaths);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    evalAndLog("store = db.createObjectStore('store', {keyPath: 'id'})");
    evalAndLog("store.createIndex('string length', 'string.length')");
    evalAndLog("store.createIndex('array length', 'array.length')");
}

function testKeyPaths()
{
    debug("");
    debug("testKeyPaths():");

    transaction = evalAndLog("transaction = db.transaction('store', 'readwrite')");
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
