if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB createObjectStore null handling");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    objectStore = evalAndLog("db.createObjectStore(null);");
    shouldBeEqualToString("objectStore.name", "null");
    finishJSTest();
}
