description("This tests if Entries returned by callbacks keep functioning even after we lose a reference to the filesystem in the script.");

var testDirectory = null;
var testEntries = null;

function errorCallback(error)
{
    testFailed("Error occurred: " + error.code);
    finishJSTest();
}

function runTest1(root)
{
    debug("Running runTest1()...");
    var helper = new JoinHelper();
    var done = function() { helper.done(); };

    helper.run(function() { root.getFile('a', {create:true}, done, errorCallback); });
    helper.run(function() { root.getDirectory('b', {create:true}, done, errorCallback); });
    helper.run(function() { root.getDirectory('c', {create:true}, done, errorCallback); });

    gc();
    helper.join(function() { runTest2(root); });
}

function runTest2(root)
{
    debug("Running runTest2()...");
    var helper = new JoinHelper();
    var done = function() { helper.done(); };

    helper.run(function() { root.getFile('/b/1', {create:true}, done, errorCallback); });
    helper.run(function() { root.getFile('/b/2', {create:true}, done, errorCallback); });
    helper.run(function() { root.getFile('/b/3', {create:true}, done, errorCallback); });
    helper.run(function() { root.getFile('/b/4', {create:true}, done, errorCallback); });
    helper.run(function() { root.getDirectory('b', {create:false}, function(entry) { testDirectory = entry; helper.done(); }, errorCallback); });

    gc();
    helper.join(function() { runTest3(); });
}

function runTest3()
{
    debug("Running runTest3()...");
    testDirectory.createReader().readEntries(entriesCallback, errorCallback);
    testDirectory = null;
    gc();
}

function entriesCallback(entries)
{
    debug("Running runTest4()...");
    testEntries = entries;
    shouldBe("testEntries.length", "4");
    finishJSTest();
}

function fileSystemCallback(fs)
{
    var root = fs.root;
    removeAllInDirectory(fs.root, function() { runTest1(root); }, errorCallback);
    fs = null;
    gc();
}

if (window.webkitRequestFileSystem) {
    window.jsTestIsAsync = true;
    webkitRequestFileSystem(window.TEMPORARY, 100, fileSystemCallback, errorCallback);
} else
    debug("This test requires FileSystem API support.");
