// Helper routine.
function toString(obj) {
    if (obj == undefined) {
        return "undefined";
    } else if (typeof obj == 'object') {
        if (obj.length != undefined) {
            var stringArray = [];
            for (var i = 0; i < obj.length; ++i)
                stringArray.push(toString(obj[i]));
            stringArray.sort();
            return '[' + stringArray.join(',\n ') + ']';
        } else if (obj.isDirectory || obj.isFile) {
            return 'ENTRY {path:' + obj.fullPath + ' name:' + obj.name + (obj.isDirectory ? ' type:DIRECTORY' : ' type:FILE') + '}';
        } else {
            return obj + "";
        }
    } else
        return obj + "";
}

// Creates a test environment for the given entries where entries is an array of {fullPath:String, isDirectory:boolean} object.
// When the world is created successfully, successCallback is called with an object that holds all the created entry objects.
function createTestEnvironment(fileSystem, entries, successCallback, errorCallback)
{
    var Helper = function(fileSystem, entries, successCallback, errorCallback)
    {
        this.fileSystem = fileSystem;
        this.entries = entries;
        this.current = 0;
        this.successCallback = successCallback;
        this.errorCallback = errorCallback;
        this.environment = {};

        this.createSuccessCallback = function(entry, size)
        {
            if (entry.isFile && size > 0) {
                entry.createWriter(bindCallback(this, function(writer) {
                    writer.truncate(size);
                    writer.onerror = bindCallback(this, this.createErrorCallback);
                    writer.onwriteend = bindCallback(this, function() {
                        this.environment[entry.fullPath] = entry;
                        this.environment[entry.fullPath + '.size'] = size;
                        this.createNextEntry();
                    });
                }), bindCallback(this, this.createErrorCallback));
                return;
            }
            this.environment[entry.fullPath] = entry;
            this.environment[entry.fullPath + '.size'] = 0;
            this.createNextEntry();
        }

        this.createErrorCallback = function(error, entry)
        {
            testFailed('Got unexpected error ' + error.code + ' while creating ' + toString(entry));
            this.errorCallback(error);
        }

        this.createNextEntry = function()
        {
            if (this.current >= this.entries.length) {
                this.successCallback(this.environment);
                return;
            }
            var entry = this.entries[this.current++];
            if (entry.isDirectory)
                fileSystem.root.getDirectory(entry.fullPath, {create:true}, bindCallback(this, this.createSuccessCallback), bindCallback(this, this.createErrorCallback));
            else
                fileSystem.root.getFile(entry.fullPath, {create:true}, bindCallback(this, this.createSuccessCallback, entry.size), bindCallback(this, this.createErrorCallback));
        };
    };

    if (!entries || !entries.length) {
        successCallback({});
        return;
    }
    var helper = new Helper(fileSystem, entries, successCallback, errorCallback);
    helper.createNextEntry();
}

function verifyTestEnvironment(fileSystem, entries, successCallback, errorCallback)
{
    var Helper = function(fileSystem, entries, successCallback, errorCallback)
    {
        this.fileSystem = fileSystem;
        this.entries = entries;
        this.current = 0;
        this.successCallback = successCallback;
        this.errorCallback = errorCallback;
        this.expectedNonexistent = false;

        this.verifySuccessCallback = function(entry)
        {
            if (this.expectedNonexistent) {
                testFailed('Found unexpected entry ' + entry.fullPath);
                this.errorCallback();
                return;
            }
            testPassed('Verified entry: ' + toString(entry));
            this.verifyNextEntry();
        }

        this.verifyErrorCallback = function(error, entry)
        {
            if (this.expectedNonexistent) {
                testPassed('Verified entry does NOT exist: ' + entry.fullPath);
                this.verifyNextEntry();
                return;
            }
            if (error == FileError.NOT_FOUND_ERR)
                testFailed('Not found: ' + entry.fullPath);
            else
                testFailed('Got unexpected error ' + error.code + ' for ' + entry.fullPath);
            this.errorCallback(error);
        }

        this.verifyNextEntry = function()
        {
            if (this.current >= this.entries.length) {
                this.successCallback();
                return;
            }
            var entry = this.entries[this.current++];
            this.expectedNonexistent = entry.nonexistent;
            if (entry.isDirectory)
                fileSystem.root.getDirectory(entry.fullPath, {}, bindCallback(this, this.verifySuccessCallback), bindCallback(this, this.verifyErrorCallback, entry));
            else
                fileSystem.root.getFile(entry.fullPath, {}, bindCallback(this, this.verifySuccessCallback), bindCallback(this, this.verifyErrorCallback, entry));
        };
    };

    if (!entries || !entries.length) {
        successCallback();
        return;
    }
    var helper = new Helper(fileSystem, entries, successCallback, errorCallback);
    helper.verifyNextEntry();
}

// testCase must have precondition, postcondition and test function field.
// (See resources/op-*.js for examples.)
function runOperationTest(fileSystem, testCase, successCallback, errorCallback)
{
    var OperationTestHelper = function(fileSystem, testCase, successCallback, errorCallback)
    {
        this.fileSystem = fileSystem;
        this.testCase = testCase;
        this.successCallback = successCallback;
        this.errorCallback = errorCallback;
        this.stage = '';
        this.environment = {};
        this.currentTest = 0;

        this.currentReader = null;
        this.readEntries = [];

        this.getSymbolString = function(symbol)
        {
            return 'this.environment["' + symbol + '"]';
        };

        this.testSuccessCallback = function()
        {
            if (!this.expectedErrorCode) {
                testPassed('Succeeded: ' + this.stage);
                this.runNextTest();
            } else
                testFailed('Unexpectedly succeeded while ' + this.stage);
        };

        this.entry = null;
        this.testGetSuccessCallback = function(entry)
        {
            if (!this.expectedErrorCode) {
                testPassed('Succeeded: ' + this.stage);
                this.entry = entry;
                shouldBe.apply(this, ['this.environment[this.entry.fullPath].fullPath', '"' + entry.fullPath + '"']);
                shouldBe.apply(this, ['this.environment[this.entry.fullPath].isFile + ""', '"' + entry.isFile + '"']);
                shouldBe.apply(this, ['this.environment[this.entry.fullPath].isDirectory + ""', '"' + entry.isDirectory + '"']);
                this.runNextTest();
            } else
                testFailed('Unexpectedly succeeded while ' + this.stage);
        };

        this.testCreateSuccessCallback = function(entry)
        {
            if (!this.expectedErrorCode) {
                testPassed('Succeeded: ' + this.stage);
                this.environment[entry.fullPath] = entry;
                this.runNextTest();
            } else
                testFailed('Unexpectedly succeeded while ' + this.stage);
        };

        this.testGetParentSuccessCallback = function(entry)
        {
            if (!this.expectedErrorCode) {
                testPassed('Succeeded: ' + this.stage);
                debug('Parent entry: ' + toString(entry));
                this.runNextTest();
            } else
                testFailed('Unexpectedly succeeded while ' + this.stage);
        };

        this.testReadEntriesSuccessCallback = function(entries)
        {
            if (this.expectedErrorCode)
                testFailed('Unexpectedly succeeded while ' + this.stage);

            for (var i = 0; i < entries.length; ++i)
                this.readEntries.push(entries[i]);

            if (entries.length) {
                this.currentReader.readEntries(bindCallback(this, this.testReadEntriesSuccessCallback), bindCallback(this, this.testErrorCallback));
                return;
            }

            testPassed('Succeeded: ' + this.stage);
            debug('Entries: ' + toString(this.readEntries));
            this.runNextTest();
        };

        this.testMetadataSuccessCallback = function(metadata, entry)
        {
            if (!this.expectedErrorCode) {
                testPassed('Succeeded: ' + this.stage);
                var symbol = entry + '.returned.modificationTime';
                this.environment[symbol] = metadata.modificationTime;
                var entryMetadataString = this.getSymbolString(symbol);
                if (entry != '/') {
                    shouldBeGreaterThanOrEqual.apply(this, [entryMetadataString, 'this.roundedStartDate']);
                }
                if (metadata.size) {
                    this.environment[entry + '.returned.size'] = metadata.size;
                    var entrySizeString = this.getSymbolString(entry + '.returned.size');
                    var expectedSizeString = this.getSymbolString(entry + '.size');
                    shouldBe.apply(this, [expectedSizeString, entrySizeString]);
                }
                this.runNextTest();
            } else
                testFailed('Unexpectedly succeeded while ' + this.stage);
        };

        this.testErrorCallback = function(error)
        {
            if (this.expectedErrorCode) {
                shouldBe.apply(this, ['this.expectedErrorCode + ""', '"' + error.code + '"']);
                this.runNextTest();
            } else {
                testFailed('Got unexpected error ' + error.code + ' while ' + this.stage);
                this.errorCallback(error);
            }
        };

        // Operations ---------------------------------------------------

        this.getFile = function(entry, path, flags, expectedErrorCode)
        {
            this.expectedErrorCode = expectedErrorCode;
            this.stage = '"' + entry + '".getFile("' + path + '")';
            var successCallback = (flags && flags.create) ? this.testCreateSuccessCallback : this.testGetSuccessCallback;
            this.environment[entry].getFile(path, flags, bindCallback(this, successCallback), bindCallback(this, this.testErrorCallback));
        };

        this.getDirectory = function(entry, path, flags, expectedErrorCode)
        {
            this.expectedErrorCode = expectedErrorCode;
            this.stage = '"' + entry + '".getDirectory("' + path + '")';
            var successCallback = (flags && flags.create) ? this.testCreateSuccessCallback : this.testGetSuccessCallback;
            this.environment[entry].getDirectory(path, flags, bindCallback(this, successCallback), bindCallback(this, this.testErrorCallback));
        };

        this.getParent = function(entry, expectedErrorCode)
        {
            this.expectedErrorCode = expectedErrorCode;
            this.stage = '"' + entry + '".getParent()';
            this.environment[entry].getParent(bindCallback(this, this.testGetParentSuccessCallback), bindCallback(this, this.testErrorCallback));
        };

        this.getMetadata = function(entry, expectedErrorCode)
        {
            this.expectedErrorCode = expectedErrorCode;
            this.stage = '"' + entry + '".getMetadata()';
            this.environment[entry].getMetadata(bindCallback(this, this.testMetadataSuccessCallback, entry), bindCallback(this, this.testErrorCallback));
        };

        this.remove = function(entry, expectedErrorCode)
        {
            this.expectedErrorCode = expectedErrorCode;
            this.stage = '"' + entry + '".remove()';
            this.environment[entry].remove(bindCallback(this, this.testSuccessCallback), bindCallback(this, this.testErrorCallback));
        };

        this.removeRecursively = function(entry, expectedErrorCode)
        {
            this.expectedErrorCode = expectedErrorCode;
            this.stage = '"' + entry + '".removeRecursively()';
            this.environment[entry].removeRecursively(bindCallback(this, this.testSuccessCallback), bindCallback(this, this.testErrorCallback));
        };

        this.readDirectory = function(entry, expectedErrorCode)
        {
            this.expectedErrorCode = expectedErrorCode;
            this.readEntries = [];
            this.stage = '"' + entry + '".createReader().readEntries()';
            this.currentReader = this.environment[entry].createReader();
            this.currentReader.readEntries(bindCallback(this, this.testReadEntriesSuccessCallback), bindCallback(this, this.testErrorCallback));
        };

        this.copy = function(entry, destinationParent, newName, expectedErrorCode)
        {
            this.expectedErrorCode = expectedErrorCode;
            this.stage = '"' + entry + '".copyTo("' + destinationParent + '", "' + newName + '")';
            this.environment[entry].copyTo(this.environment[destinationParent], newName, bindCallback(this, this.testSuccessCallback), bindCallback(this, this.testErrorCallback));
        };

        this.move = function(entry, destinationParent, newName, expectedErrorCode)
        {
            this.expectedErrorCode = expectedErrorCode;
            this.stage = '"' + entry + '".moveTo("' + destinationParent + '", "' + newName + '")';
            this.environment[entry].moveTo(this.environment[destinationParent], newName, bindCallback(this, this.testSuccessCallback), bindCallback(this, this.testErrorCallback));
        };

        this.shouldBe = function(symbol1, symbol2)
        {
            shouldBe.apply(this, [this.getSymbolString(symbol1), this.getSymbolString(symbol2)]);
            this.runNextTest();
        };

        this.shouldBeGreaterThanOrEqual = function(symbol1, symbol2)
        {
            shouldBeGreaterThanOrEqual.apply(this, [this.getSymbolString(symbol1), this.getSymbolString(symbol2)]);
            this.runNextTest();
        };

        //---------------------------------------------------------------
        this.start = function()
        {
            this.expectedErrorCode = '';
            this.stage = 'resetting filesystem';
            // Record rounded start date (current time minus 999 msec) here for the comparison. Entry.getMetadata() may return the last mod time in seconds accuracy while new Date() is milliseconds accuracy.
            this.roundedStartDate = new Date((new Date()).getTime() - 999);
            removeAllInDirectory(this.fileSystem.root, bindCallback(this, this.setUp), bindCallback(this, this.testErrorCallback));
        };

        this.setUp = function()
        {
            this.expectedErrorCode = '';
            this.stage = 'setting up test precondition';
            createTestEnvironment(this.fileSystem, this.testCase.precondition, bindCallback(this, this.runTests), bindCallback(this, this.testErrorCallback));
        };

        this.runNextTest = function()
        {
            if (this.currentTest >= this.testCase.tests.length) {
                this.verify();
                return;
            }
            this.testCase.tests[this.currentTest++](this);
        };

        this.runTests = function(environment)
        {
            this.environment = environment;
            this.environment['/'] = this.fileSystem.root;
            this.currentTest = 0;
            this.runNextTest();
        };

        this.verify = function()
        {
            this.expectedErrorCode = '';
            if (!this.testCase.postcondition) {
                this.successCallback();
                return;
            }
            this.stage = 'verifying test postcondition';
            verifyTestEnvironment(this.fileSystem, this.testCase.postcondition, this.successCallback, bindCallback(this, this.testErrorCallback));
        };
    };

    var helper = new OperationTestHelper(fileSystem, testCase, successCallback, errorCallback);
    helper.start();
}

var currentTest = 0;
var fileSystem = null;
function runNextTest() {
    if (currentTest >= testCases.length) {
        debug('Finished running tests.');
        finishJSTest();
        return;
    }

    var testCase = testCases[currentTest++];
    debug('* Running: ' + testCase.name);
    runOperationTest(fileSystem, testCase, runNextTest, errorCallback);
}

function errorCallback(error) {
    if (error && error.code)
        testFailed('Got error ' + error.code);
    finishJSTest();
}

function fileSystemCallback(fs)
{
    fileSystem = fs;
    runNextTest();
}

var jsTestIsAsync = true;
webkitRequestFileSystem(TEMPORARY, 100, fileSystemCallback, errorCallback);
