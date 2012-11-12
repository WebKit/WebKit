if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
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
