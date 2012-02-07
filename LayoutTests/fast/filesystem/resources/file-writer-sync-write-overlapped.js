if (this.importScripts) {
    importScripts('../resources/fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('../resources/fs-test-util.js');
    importScripts('../resources/file-writer-utils.js');
}

description("Test using FileWriterSync.seek to write overlapping existing data in a file.");

// Start with an empty FileSystem.
var fileSystem = webkitRequestFileSystemSync(this.TEMPORARY, 100);
removeAllInDirectorySync(fileSystem.root);

// Prepare a file with sample contents.
var entry = fileSystem.root.getFile('a', {create:true, exclusive:true});
var writer = entry.createWriter();
assert(!writer.position);
var builder = new WebKitBlobBuilder();
var testData = "test data";
builder.append(testData);
var blob = builder.getBlob();
writer.write(blob);
assert(writer.length == testData.length);
assert(writer.position == writer.length);

// Seek back to from the end, then overwrite, extending the file.
var extensionOffset = -4;
writer.seek(extensionOffset);
writer.write(blob);

// Verify the contents.
assert(writer.length == 2 * testData.length + extensionOffset);
assert(writer.position == writer.length);
var file = entry.file();
var reader = new FileReaderSync();
var contents = reader.readAsBinaryString(file);
var i;
for (i = 0; i < testData.length + extensionOffset; ++i)
    assert(contents.charCodeAt(i) == testData.charCodeAt(i));
var j;
for (j = 0; i < writer.length; ++i, ++j)
    assert(contents.charCodeAt(i) == testData.charCodeAt(j));
testPassed("Overlapped write 1 verified.");

// Truncate back to empty and reset the contents.
writer.truncate(0);
assert(!writer.position);
assert(!writer.length);
writer.write(blob);
assert(writer.length == testData.length);
assert(writer.position == writer.length);

// Seek forward from the beginning, then overwrite, extending the file.
extensionOffset = 4;
writer.seek(extensionOffset);
writer.write(blob);
assert(writer.length == testData.length + extensionOffset);
assert(writer.position == writer.length);

// Verify the contents.
contents = reader.readAsBinaryString(file);
for (i = 0; i < extensionOffset; ++i)
    assert(contents.charCodeAt(i) == testData.charCodeAt(i));
for (j = 0; i < writer.length; ++i, ++j)
    assert(contents.charCodeAt(i) == testData.charCodeAt(j));
testPassed("Overlapped write 2 verified.");

// Truncate back to empty and reset the contents.
writer.truncate(0);
assert(!writer.position);
assert(!writer.length);
writer.write(blob);
writer.write(blob);
assert(writer.length == 2 * testData.length);
assert(writer.position == writer.length);

// Seek forward from the beginning, then overwrite in the middle of the file.
extensionOffset = 4;
writer.seek(extensionOffset);
writer.write(blob);
assert(writer.length == 2 * testData.length);
assert(writer.position == testData.length + extensionOffset);

// Verify the contents.
contents = reader.readAsBinaryString(file);
for (i = 0; i < extensionOffset; ++i)
    assert(contents.charCodeAt(i) == testData.charCodeAt(i));
for (j = 0; i < testData.length + extensionOffset; ++i, ++j)
    assert(contents.charCodeAt(i) == testData.charCodeAt(j));
for (j = extensionOffset; i < writer.length; ++i, ++j)
    assert(contents.charCodeAt(i) == testData.charCodeAt(j));
testPassed("Overlapped write 3 verified.");
finishJSTest();
