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
        testFailed(property + " value was changed to " + newValue);
    }
}

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");
    setReadonlyProperty("objectStore.transaction", "this");
    finishJSTest();
}
