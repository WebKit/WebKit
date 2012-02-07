if (this.importScripts) {
    importScripts('../resources/fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('../resources/fs-test-util.js');
}

description("webkitRequestFileSystemSync PERSISTENT test.");

var fileSystem = webkitRequestFileSystemSync(PERSISTENT, 100);

debug("Successfully obtained PERSISTENT FileSystem:" + fileSystem.name);
shouldBeTrue("fileSystem.name.length > 0");
shouldBe("fileSystem.root.fullPath", '"/"');
finishJSTest();
