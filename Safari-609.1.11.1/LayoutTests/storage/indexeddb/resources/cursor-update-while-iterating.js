if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's cursor iteration with update and deletion.");

indexedDBTest(prepareDatabase, onOpenSuccess);

const objectArray = [
    { key: "key1", value: "value1" },
    { key: "key1", value: "value1" },
    { key: "key1", value: "value3" },
    { key: "key2", value: "value2" },
    { key: "key2", value: "value4" }
];

function populateObjectStore() {
    for (let object of objectArray)
        objectStore.add(object).onerror = unexpectedErrorCallback;
}

function prepareDatabase(event)
{
    preamble(event);
    evalAndLog("db = event.target.result");
    deleteAllObjectStores(db);

    objectStore = evalAndLog("objectStore = db.createObjectStore('objectStore', {autoIncrement: true})");
    evalAndLog("objectStore.createIndex('key', 'key', {unique: false})");

    populateObjectStore();
}

function onOpenSuccess(event)
{
    preamble(event);
    evalAndLog("db = event.target.result");
    t = evalAndLog("t = db.transaction('objectStore', 'readwrite')");
    t.oncomplete = () => { finishJSTest(); }

    evalAndLog("objectStore = t.objectStore('objectStore')");
    evalAndLog("index = objectStore.index('key')");
    request = evalAndLog("index.openCursor()");

    var n = 0;
    request.onsuccess = function(event) {
        cursor = event.target.result;
        if (cursor) {
            shouldBeEqualToString("JSON.stringify(cursor.value)", JSON.stringify(objectArray[n++]));

            if (cursor.key == "key1") {
                debug("Update cursor");
                const {value} = cursor;
                cursor.update(value);
            } else {
                debug("Delete cursor");
                cursor.delete();
            }

            debug("Cursor continues\n");
            cursor.continue();
        } else {
            if (n != objectArray.length)
                testFailed("Cursor didn't go through whole array.");
            else 
                testPassed("Successfully iterated whole array with cursor updates.");
        }
    }

    request.onerror = unexpectedErrorCallback;
}