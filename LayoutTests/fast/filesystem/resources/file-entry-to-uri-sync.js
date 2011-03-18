importScripts('fs-worker-common.js');

description("Obtaining URL from FileEntry.");

var fileSystem = requestFileSystemSync(TEMPORARY, 100);
removeAllInDirectorySync(fileSystem.root);

var testFileName = 'testFileEntry.txt';
var testFileEntry = fileSystem.root.getFile(testFileName, {create:true});

shouldBe("testFileEntry.toURL()", "'filesystem:file:///temporary/testFileEntry.txt'");

removeAllInDirectorySync(fileSystem.root);
finishJSTest();
var successfullyParsed = true;
