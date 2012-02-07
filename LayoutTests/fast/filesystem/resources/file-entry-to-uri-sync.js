importScripts('fs-worker-common.js');
importScripts('../../js/resources/js-test-pre.js');

description("Obtaining URL from FileEntry.");

var fileSystem = webkitRequestFileSystemSync(TEMPORARY, 100);
removeAllInDirectorySync(fileSystem.root);

var testFileName = 'testFileEntry.txt';
var testFileEntry = fileSystem.root.getFile(testFileName, {create:true});

shouldBe("testFileEntry.toURL()", "'filesystem:file:///temporary/testFileEntry.txt'");

removeAllInDirectorySync(fileSystem.root);
finishJSTest();
