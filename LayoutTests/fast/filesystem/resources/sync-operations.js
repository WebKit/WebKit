if (this.importScripts) {
    importScripts('../resources/fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('../resources/fs-test-util.js');
}

description("Test making multiple synchronous FileSystem operations.");

var fileSystem = webkitRequestFileSystemSync(this.TEMPORARY, 100);
removeAllInDirectorySync(fileSystem.root);

// Stage 1 (prepare)
var a = fileSystem.root.getFile('a', {create:true});
var b = fileSystem.root.getDirectory('b', {create:true});
var c = fileSystem.root.getDirectory('c', {create:true});
var d = fileSystem.root.getFile('d', {create:true});

// Stage 2 (test)
var a_copy = a.copyTo(b, 'tmp');
var metadata = a.getMetadata();
var b_parent = b.getParent();
var c_copy = c.copyTo(fileSystem.root, 'f');
var d_new = d.moveTo(fileSystem.root, 'd2');
var e = fileSystem.root.getFile('e', {create:true});

// Verify
var reader = fileSystem.root.createReader();
var dirsCount = 0;
var paths = [];
do {
    var entries = reader.readEntries();
    for (var i = 0; i < entries.length; ++i) {
        paths.push(entries[i].fullPath);
        if (entries[i].isDirectory)
            dirsCount++;
    }
} while (entries.length);

paths.sort();
shouldBe('"' + paths.join(',') + '"', '"/a,/b,/c,/d2,/e,/f"');
shouldBe("dirsCount", "3");
removeAllInDirectorySync(fileSystem.root);
finishJSTest();
