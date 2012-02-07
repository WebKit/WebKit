if (this.importScripts) {
    importScripts('../resources/fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('../resources/fs-test-util.js');
}

description("requestFileSystem TEMPORARY test.");

var fileSystem = null;

function errorCallback(error) {
    debug('Error occured while requesting a TEMPORARY file system:' + error.code);
    finishJSTest();
}

function successCallback(fs) {
    fileSystem = fs;
    debug("Successfully obtained TEMPORARY FileSystem:" + fileSystem.name);
    shouldBeTrue("fileSystem.name.length > 0");
    shouldBe("fileSystem.root.fullPath", '"/"');
    finishJSTest();
}

var jsTestIsAsync = true;
webkitRequestFileSystem(TEMPORARY, 100, successCallback, errorCallback);
