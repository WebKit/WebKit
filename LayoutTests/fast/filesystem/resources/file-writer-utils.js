function stringifyObj(o)
{
    s = "";
    if (o)
        for (index in o) {
            s += index + ": " + o[index] + ";";
        }
    return s;
}

function onError(e)
{
    testFailed("Got error: " + stringifyObj(e));
    cleanUp();
}

function assert(s)
{
    if (!s) {
        var e = new Error("Assertion failed. ");
        onError(e);
        throw e;
    }
}

var fileEntryForCleanup;  // Set as soon as we have one.

function cleanUp()
{
    if (fileEntryForCleanup) {
        try {
            fileEntryForCleanup.remove(finishJSTest, finishJSTest);
            return;
        } catch (ex) {
        }
    }
    finishJSTest();
}

// Generic function that gets a File from a FileEntry and calls a custom verification function on it.
function verifyFileContents(fileEntry, verifyFunc, arg0, arg1, onSuccess)
{
    fileEntry.file(
        function(file) {
            verifyFunc(file, arg0, arg1, onSuccess);
        },
        onError);
}

// Helper function used with verifyFileContents.
function verifyFileLengthHelper(file, length, unused, onSuccess)
{
    assert(file.size == length);
    onSuccess();
}

// Verifies that the contents of fileEntry have the supplied length.
function verifyFileLength(fileEntry, length, onSuccess)
{
    verifyFileContents(fileEntry, verifyFileLengthHelper, length, null, onSuccess);
}

// Helper function used with verifyFileContents.
function verifyByteRangeIsZeroesHelper(file, start, length, onSuccess)
{
    var fileReader = new FileReader();
    fileReader.onerror = onError;
    fileReader.onload =
        function() {
            var result = fileReader.result;
            for (var i = 0; i < length; i++)
                assert(result.charCodeAt(i) == 0);
            onSuccess();
        };
    fileReader.readAsBinaryString(file.slice(start, start + length));
}

// Verifies that fileEntry, at offset, is all zeroes for length bytes.
function verifyByteRangeIsZeroes(fileEntry, offset, length, onSuccess)
{
    verifyFileContents(fileEntry, verifyByteRangeIsZeroesHelper, offset, length, onSuccess);
}

// Helper function used with verifyFileContents.
function verifyByteRangeAsStringHelper(file, start, data, onSuccess)
{
    var fileReader = new FileReader();
    fileReader.onerror = onError;
    fileReader.onload =
        function() {
            assert(fileReader.result == data);
            onSuccess();
        };
    fileReader.readAsText(file.slice(start, start + data.length));
}

// Verifies that the contents of fileEntry, at offset, match contents [a string].
function verifyByteRangeAsString(fileEntry, offset, contents, onSuccess)
{
    verifyFileContents(fileEntry, verifyByteRangeAsStringHelper, offset, contents, onSuccess);
}

// Creates a file called fileName in fileSystem's root directory, truncates it to zero length just in case, and calls onSuccess, passing it a FileEntry and FileWriter for the new file.
function createEmptyFile(fileSystem, fileName, onSuccess)
{
    function getSuccessFunc(fileEntry) {
        // Create a fresh FileWriter here, rather than pass in the used one.
        // This way we don't accidentally leave our event handlers attached.
        return function() {
            fileEntry.createWriter(function(fw) {
                onSuccess(fileEntry, fw);
            }, onError);
        }
    }
    function getFileWriterCallback(fileEntry) {
        return function(fileWriter) {
            var successFunc = getSuccessFunc(fileEntry);
            fileWriter.onError = onError;
            fileWriter.onwrite = function() {
                fileWriter.onwrite = null;
                verifyFileLength(fileEntry, 0, successFunc);
            };
            fileWriter.truncate(0);
        }
    }
    function onFileEntry(fileEntry) {
        fileEntryForCleanup = fileEntry;
        var onFileWriter = getFileWriterCallback(fileEntry);
        fileEntry.createWriter(onFileWriter, onError);
    }
    assert(fileSystem);
    fileSystem.root.getFile(fileName, {create:true}, onFileEntry, onError);
}

function writeString(fileEntry, fileWriter, offset, data, onSuccess)
{
    var blob = new Blob([data]);
    fileWriter.seek(offset);
    fileWriter.write(blob);
    fileWriter.onerror = onError;
    if (offset < 0)
        offset += fileWriter.length;
    assert(offset >= 0);
    fileWriter.onwrite = function() {
        fileWriter.onwrite = null;
        verifyByteRangeAsString(fileEntry, offset, data, onSuccess);
    };
}

function setFileContents(fileEntry, fileWriter, contents, onSuccess) {
    fileWriter.onerror = onError;
    fileWriter.onwrite = function() {
        fileWriter.onwrite = null;
        writeString(fileEntry, fileWriter, 0, contents, onSuccess);
    };
    fileWriter.truncate(0);
}

function setupAndRunTest(size, testName, testFunc)
{
    if (!webkitRequestFileSystem) {
        debug("This test requires FileSystem API support.");
        return;
    }
    debug("starting test");
    webkitRequestFileSystem(TEMPORARY, size, function(fs) {
        createEmptyFile(fs, testName, testFunc);
    }, onError);
}
