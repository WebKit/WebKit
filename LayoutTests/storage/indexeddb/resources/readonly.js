if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB readonly properties");

function setReadonlyProperty(property, value)
{
    oldValue = eval(property);
    debug("trying to set readonly property " + property);
    evalAndLog(property + " = " + value);
    newValue = eval(property);
    if (oldValue == newValue) {
        testPassed(property + " is still " + oldValue);
    } else {
        testFailed(property + " value was changed");
    }
}

function test()
{
    removeVendorPrefixes();

    name = "foo";
    request = evalAndLog("indexedDB.open(name)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    setReadonlyProperty("request.result", "Infinity");
    setReadonlyProperty("request.errorCode", "666");
    setReadonlyProperty("request.error", "{}");
    setReadonlyProperty("request.source", "this");
    setReadonlyProperty("request.transaction", "this");
    setReadonlyProperty("request.readyState", "666");

    db = evalAndLog("db = event.target.result");
    setReadonlyProperty("db.name", "'bar'");

    request = evalAndLog("request = db.setVersion('1')");
    request.onsuccess = createAndPopulateObjectStore;
    request.onerror = unexpectedErrorCallback;
}

function createAndPopulateObjectStore()
{
    transaction = evalAndLog("transaction = event.target.result;");
    setReadonlyProperty("transaction.mode", "666");
    setReadonlyProperty("transaction.db", "this");

    deleteAllObjectStores(db);

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");

    setReadonlyProperty("objectStore.name", "'bar'");
    setReadonlyProperty("objectStore.keyPath", "'bar'");
/* fails, split into separate test
    setReadonlyProperty("objectStore.indexNames", "['bar']");
*/
/* fails, split into separate test
    setReadonlyProperty("objectStore.transaction", "this");
*/

    result = evalAndLog("result = objectStore.add({}, 'first');");
    result.onerror = unexpectedErrorCallback;
    result.onsuccess = addSuccess;
}

function addSuccess()
{
    result = evalAndLog("result = objectStore.openCursor();");
    result.onerror = unexpectedErrorCallback;
    result.onsuccess = checkCursor;
}

function checkCursor()
{
    cursor = evalAndLog("cursor = event.target.result;");
    if (cursor) {
        setReadonlyProperty("cursor.source", "this");
        setReadonlyProperty("cursor.direction", "666");
        setReadonlyProperty("cursor.key", "Infinity");
        setReadonlyProperty("cursor.primaryKey", "Infinity");
    } else {
        testFailed("cursor is null");
    }

    index = evalAndLog("index = objectStore.createIndex('first', 'first');");
    setReadonlyProperty("index.name", "'bar'");
    setReadonlyProperty("index.objectStore", "this");
    setReadonlyProperty("index.keyPath", "'bar'");
    setReadonlyProperty("index.unique", "true");

    keyRange = IDBKeyRange.only("first");
    setReadonlyProperty("keyRange.lower", "Infinity");
    setReadonlyProperty("keyRange.upper", "Infinity");
    setReadonlyProperty("keyRange.lowerOpen", "true");
    setReadonlyProperty("keyRange.upperOpen", "true");

    finishJSTest();
}

test();