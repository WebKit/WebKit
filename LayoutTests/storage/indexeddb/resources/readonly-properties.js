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
        testFailed(property + " value was changed to " + newValue);
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
    db = evalAndLog("db = event.target.result");
    request = evalAndLog("request = db.setVersion('1')");
    request.onsuccess = createAndPopulateObjectStore;
    request.onerror = unexpectedErrorCallback;
}

function createAndPopulateObjectStore()
{
    transaction = evalAndLog("transaction = event.target.result;");
    deleteAllObjectStores(db);
    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");
    setReadonlyProperty("objectStore.transaction", "this");
    finishJSTest();
}

test();