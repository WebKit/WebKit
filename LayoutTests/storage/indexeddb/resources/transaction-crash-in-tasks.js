if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Regression test for http://webkit.org/b/102283");

test();
function test() {
    removeVendorPrefixes();
    setDBNameFromPath();

    evalAndLog("indexedDB.open(dbname, 2)");
    evalAndLog("indexedDB.open(dbname, 3)");

    finishJSTest();
}
