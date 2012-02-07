if (this.importScripts) {
    importScripts('../resources/fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('../resources/fs-test-util.js');
}

description("Test making multiple async FileSystem operations.");

var fileSystem = null;

var a;
var b;
var c;
var d;

var paths = [];
var dirsCount = 0;
var reader = null;

function errorCallback(error) {
    debug("Got error: " + error.code);
    removeAllInDirectory(fileSystem.root);
    finishJSTest();
}

function entriesCallback(entries) {
    for (var i = 0; i < entries.length; ++i) {
        paths.push(entries[i].fullPath);
        if (entries[i].isDirectory)
            dirsCount++;
    }
    if (!entries.length) {
        reader.readEntries(entriesCallback, errorCallback);
    } else {
        paths.sort();
        shouldBe('"' + paths.join(',') + '"', '"/a,/b,/c,/d2,/e,/f"');
        shouldBe("dirsCount", "3");
        removeAllInDirectory(fileSystem.root);
        finishJSTest();
    }
}

function verify() {
    debug("Verifying the FileSystem status.");
    reader = fileSystem.root.createReader();
    reader.readEntries(entriesCallback, errorCallback);
}

function asyncTest2() {
    debug("Starting async test stage 2.");

    var helper = new JoinHelper();
    var done = function() { helper.done(); };

    helper.run(function() { a.copyTo(b, 'tmp', done, errorCallback); });
    helper.run(function() { a.getMetadata(done, errorCallback); });
    helper.run(function() { b.getParent(done, errorCallback); });
    helper.run(function() { c.copyTo(fileSystem.root, 'f', done, errorCallback); });
    helper.run(function() { d.moveTo(fileSystem.root, 'd2', done, errorCallback); });
    helper.run(function() { fileSystem.root.getFile('e', {create:true}, done, errorCallback); });

    helper.join(verify);
}

function asyncTest1() {
    debug("Starting async test stage 1.");

    var helper = new JoinHelper();
    var root = fileSystem.root;

    helper.run(function() { root.getFile('a', {create:true}, function(entry) {
        a = entry;
        helper.done();
    }, errorCallback); });

    helper.run(function() { root.getDirectory('b', {create:true}, function(entry) {
        b = entry;
        helper.done();
    }, errorCallback); });

    helper.run(function() { root.getDirectory('c', {create:true}, function(entry) {
        c = entry;
        helper.done();
    }, errorCallback); });

    helper.run(function() { root.getFile('d', {create:true}, function(entry) {
        d = entry;
        helper.done();
    }, errorCallback); });

    helper.join(asyncTest2);
}

if (this.webkitRequestFileSystem) {
    jsTestIsAsync = true;
    var helper = new JoinHelper();
    helper.run(function() {
    webkitRequestFileSystem.apply(this, [this.TEMPORARY, 100, function(fs) {
        debug("Got FileSystem:" + fs.name);
        fileSystem = fs;
        removeAllInDirectory(fileSystem.root, function(){ helper.done(); }, errorCallback);
    }, errorCallback]); });
    debug("requested FileSystem.");
    helper.join(asyncTest1);
} else
    debug("This test requires FileSystem API support.");
