if (this.importScripts) {
    importScripts('../resources/fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('../resources/fs-test-util.js');
}

description("requestFileSystemSync TEMPORARY test.");

var fileSystem = webkitRequestFileSystemSync(TEMPORARY, 100);

debug("Successfully obtained TEMPORARY FileSystem:" + fileSystem.name);
shouldBeTrue("fileSystem.name.length > 0");
shouldBe("fileSystem.root.fullPath", '"/"');
finishJSTest();
