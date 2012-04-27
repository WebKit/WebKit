if (this.importScripts) {
    importScripts('../resources/fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('../resources/fs-test-util.js');
    importScripts('../resources/file-writer-utils.js');
}

description("Test using FileWriterSync.truncate to extend a file.");

// Start with an empty FileSystem.
var fileSystem = webkitRequestFileSystemSync(this.TEMPORARY, 100);
removeAllInDirectorySync(fileSystem.root);

// Prepare a file with sample contents.
var entry = fileSystem.root.getFile('a', {create:true, exclusive:true});
var writer = entry.createWriter();
assert(!writer.position);
var testData = "test data";
writer.write(new Blob([testData]));
assert(writer.length == testData.length);
assert(writer.position == writer.length);

// Extend the file via truncate.
var extensionLength = 10;
writer.truncate(writer.length + extensionLength);

// Verify the contents.
assert(writer.length == testData.length + extensionLength);
assert(writer.position == testData.length);
var file = entry.file();
var reader = new FileReaderSync();
var contents = reader.readAsBinaryString(file);
var i;
for (i = 0; i < testData.length; ++i)
    assert(contents.charCodeAt(i) == testData.charCodeAt(i));
for (; i < writer.length; ++i)
    assert(!contents.charCodeAt(i));

testPassed("Truncate extension verified.");
finishJSTest();
