if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
}

description("This test ensures StorageManager API works in nested dedicated workers.");

var estimate = null;
var nestedWorker = new Worker('nested-dedicated-workers2.js');
nestedWorker.onmessage = (event) => {
    estimate = event.data;
    shouldBeNonNull("estimate.quota");
    shouldBeNonNull("estimate.usage");
    finishJSTest();
};

nestedWorker.onerror = (event) => {
    testFailed('nestedWorker has error ' + event.message());
    finishJSTest();
}
