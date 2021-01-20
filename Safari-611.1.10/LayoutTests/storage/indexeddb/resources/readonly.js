if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
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

indexedDBTest(prepareDatabase, openSuccess);
function prepareDatabase()
{
    db = event.target.result;
    transaction = evalAndLog("transaction = event.target.transaction;");
    setReadonlyProperty("transaction.mode", "666");
    setReadonlyProperty("transaction.db", "this");

    deleteAllObjectStores(db);

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");

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
    setReadonlyProperty("index.objectStore", "this");
    setReadonlyProperty("index.keyPath", "'bar'");
    setReadonlyProperty("index.unique", "true");

    keyRange = IDBKeyRange.only("first");
    setReadonlyProperty("keyRange.lower", "Infinity");
    setReadonlyProperty("keyRange.upper", "Infinity");
    setReadonlyProperty("keyRange.lowerOpen", "true");
    setReadonlyProperty("keyRange.upperOpen", "true");
}

function openSuccess()
{
    request = event.target;
    setReadonlyProperty("request.result", "Infinity");
    setReadonlyProperty("request.error", "{}");
    setReadonlyProperty("request.source", "this");
    setReadonlyProperty("request.transaction", "this");
    setReadonlyProperty("request.readyState", "666");

    db = evalAndLog("db = event.target.result");
    setReadonlyProperty("db.name", "'bar'");

    finishJSTest();
}
