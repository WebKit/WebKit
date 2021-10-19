if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
}

description('This test checks iteration on FileSystemDirectoryHandle.');

const handleTypes = ['directory', 'directory', 'file', 'file'];
const handleNames = ['dir1', 'dir2', 'file1', 'file2'];
var index, isEqual, iterator, prototype, descriptor, resultKeys, resultValuesName, resultValuesType, resultEntries, nextResults, expectedResults, nextResultsEnd;

function finishTest(error)
{
    if (error) {
        testFailed(error);
    }
    finishJSTest();
}

function testPrototype(it)
{
    iterator = it;
    shouldBeTrue('typeof iterator[Symbol.asyncIterator] === "function"');
    var iteratorPrototype = Object.getPrototypeOf(iterator);
    prototype = Object.getPrototypeOf(iteratorPrototype);
    descriptor = Object.getOwnPropertyDescriptor(prototype, Symbol.asyncIterator);
    shouldBeTrue('descriptor.configurable');
    shouldBeTrue('descriptor.writable');
    shouldBeFalse('descriptor.enumerable');
}

async function testNext(it, expected, sortFunction, compareFunction)
{
    nextResults = [];
    expectedResults = expected;
    for (index = 0; index < expected.length; index++) {
        var result = await it.next();
        nextResults.push(result.value);
    }
    nextResults.sort(sortFunction);
    if (!compareFunction) {
        shouldBe('nextResults', 'expectedResults');
    } else {
        shouldBe('nextResults.length', 'expectedResults.length');
        isEqual = compareFunction;
        for (index = 0; index < expectedResults.length; index++) {
            shouldBeTrue('isEqual(expectedResults[index], nextResults[index])');
        }
    }
    nextResultsEnd = await it.next();
    shouldBeTrue('nextResultsEnd.done');
    shouldBeUndefined('nextResultsEnd.value');
}

async function test() 
{
    try {
        // Create a new directory for this test.
        var originHandle = await navigator.storage.getDirectory();
        await originHandle.removeEntry('directory-handle-iteration', { 'recursive' : true }).then(() => { }, () => { });
        var rootHandle = await originHandle.getDirectoryHandle('directory-handle-iteration', { 'create' : true });

        for (let i = 0; i < handleNames.length; i++) {
            if (handleTypes[i] == 'directory') {
                handle = await rootHandle.getDirectoryHandle(handleNames[i], { 'create' : true });
            } else {
                handle = await rootHandle.getFileHandle(handleNames[i], { 'create' : true });
            }
        }

        debug('Test keys():');
        var keys = [];
        for await (key of rootHandle.keys()) {
            keys.push(key);
        }
        var sortFunc = (a, b) => { return (a > b) ? 1 : -1 };
        keys.sort(sortFunc);
        resultKeys = keys;
        shouldBe('resultKeys.length', 'handleNames.length');
        shouldBe('resultKeys', 'handleNames');
        var keysIterator = rootHandle.keys();
        testPrototype(keysIterator);
        await testNext(keysIterator, keys, sortFunc);

        debug('Test values():');
        var values = [];
        for await (value of rootHandle.values()) {
            values.push(value);
        }
        sortFunc = (a, b) => { return (a.name > b.name) ? 1 : -1 };
        values.sort(sortFunc);
        var valuesName = [], valuesType = [];
        values.forEach(value => {
            valuesName.push(value.name);
            valuesType.push(value.kind);
        });
        resultValuesName = valuesName;
        resultValuesType = valuesType;
        shouldBe('resultValuesName.length', 'handleNames.length');
        shouldBe('resultValuesName', 'handleNames');
        shouldBe('resultValuesType', 'handleTypes');
        var valuesIterator = rootHandle.values();
        testPrototype(valuesIterator);
        var compareValue = (a, b) => { return a.name == b.name && a.kind == b.kind; };
        await testNext(valuesIterator, values, sortFunc, compareValue);

        debug('Test entries():');
        var entriesIterator = rootHandle.entries();
        testPrototype(entriesIterator);
        var entries = [];
        for await (entry of entriesIterator) {
            entries.push(entry);
        }
        sortFunc = (a, b) => { return (a[0] > b[0]) ? 1 : -1 };
        entries.sort(sortFunc);
        var entriesKey = [], entriesValueName = [], entriesValueType = [];
        entries.forEach(entry => {
            entriesKey.push(entry[0]);
            entriesValueName.push(entry[1].name);
            entriesValueType.push(entry[1].kind);
        });
        resultKeys = entriesKey;
        resultValuesName = entriesValueName;
        resultValuesType = entriesValueType;
        shouldBe('resultKeys.length', 'handleNames.length');
        shouldBe('resultKeys', 'handleNames');
        shouldBe('resultValuesName', 'handleNames');
        shouldBe('resultValuesType', 'handleTypes');
        var entriesIterator = rootHandle.entries();
        testPrototype(entriesIterator);
        var compareEntry = (a, b) => { return a[0] == b[0] && compareValue(a[1], b[1]); };
        await testNext(entriesIterator, entries, sortFunc, compareEntry);

        finishTest();
    } catch (error) {
        finishTest(error);
    }
}

test();
