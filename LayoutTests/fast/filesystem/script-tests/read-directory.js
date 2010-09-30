description("DirectoryReader.readEntries() test.");
var fileSystem = null;
var reader = null;
var resultEntries = [];

var readEntriesCount = 0;
var entriesCallbackCount = 0;

// path: isDirectory map.
var testEntries = {
    '/a': true,
    '/b': false,
    '/c': true,
    '/d': false,
    '/e': false,
    '/f': true,
};
var testEntriesCount = 0;

function endTest()
{
    removeRecursively(fileSystem.root);
    finishJSTest();
}

function errorCallback(error)
{
    debug("Error occured:" + error.code);
    endTest();
}

var entry = null;
function verifyTestResult()
{
    shouldBe('readEntriesCount', 'entriesCallbackCount');
    shouldBe('resultEntries.length', 'testEntriesCount');
    resultEntries.sort(function(a, b) { return (a.fullPath > b.fullPath) ? 1 : (a.fullPath < b.fullPath) ? -1 : 0; });
    for (i = 0; i < resultEntries.length; ++i) {
        entry = resultEntries[i];
        debug('Entry:' + entry.fullPath + ' isDirectory:' + entry.isDirectory);
        shouldBe('testEntries[entry.fullPath]', 'entry.isDirectory');
    }
}

function entriesCallback(entries)
{
    entriesCallbackCount++;

    for (var i = 0; i < entries.length; ++i)
        resultEntries.push(entries[i]);

    if (entries.length) {
        readEntriesCount++;
        reader.readEntries(entriesCallback, errorCallback);
    } else {
        // Must have read all the entries.
        verifyTestResult();
        endTest();
    }
}

function runReadDirectoryTest()
{
    readEntriesCount++;
    reader = fileSystem.root.createReader();
    reader.readEntries(entriesCallback, errorCallback);
}

function prepareForTest()
{
    var helper = new JoinHelper();
    var done = function() { helper.done(); };

    for (var path in testEntries) {
        testEntriesCount++;
        if (testEntries[path])
            helper.run(function() { fileSystem.root.getDirectory(path, {create:true}, done, errorCallback); });
        else
            helper.run(function() { fileSystem.root.getFile(path, {create:true}, done, errorCallback); });
    }
    helper.join(runReadDirectoryTest);
}

function successCallback(fs)
{
    fileSystem = fs;
    debug("Successfully obtained Persistent FileSystem:" + fileSystem.name);
    removeRecursively(fileSystem.root, prepareForTest, errorCallback);
}

if (window.requestFileSystem) {
    requestFileSystem(window.TEMPORARY, 100, successCallback, errorCallback);
    window.jsTestIsAsync = true;
} else
    debug("This test requires FileSystem API support.");

window.successfullyParsed = true;
