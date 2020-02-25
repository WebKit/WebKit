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
    { key: "key2", value: "value4" },
	{ key: "key3", value: "value5" },
	{ key: "key3", value: "value6" },
];

function populateObjectStore() {
    objectArray.forEach((object, i)=>{
        objectStore.add(object, i).onerror = unexpectedErrorCallback;
	});
}

function prepareDatabase(event)
{
    preamble(event);
    evalAndLog("db = event.target.result");
    deleteAllObjectStores(db);

    objectStore = evalAndLog("objectStore = db.createObjectStore('objectStore')");
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

    totalRecordCount = 0;
    request.onsuccess = function(event) {
        cursor = event.target.result;
        if (cursor) {
            shouldBeEqualToString("JSON.stringify(cursor.value)", JSON.stringify(objectArray[totalRecordCount++]));

            if (cursor.key == "key1") {
                debug("Update cursor");
                const {value} = cursor;
                cursor.update(value);
            }
            if (cursor.key == "key2") {
                debug("Delete cursor");
                cursor.delete();
            }
            if (cursor.key == "key3") {
                debug("Delete last record");
                objectStore.delete(6);
            }

            debug("Cursor continues\n");
            cursor.continue();
        } else {
            shouldBeEqualToNumber("totalRecordCount", objectArray.length - 1);
        }
    }

    request.onerror = unexpectedErrorCallback;
}