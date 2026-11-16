const verbose = false;
const supportsReplaceAll = (String.prototype.replaceAll != undefined);

var visits;
var failures = 0;

if (console)
    print = console.log;

function verify(desc, testName, result, visits, expected) {
    var failed = (result != expected.result);
    if (failed || verbose) print(desc, " ", testName, ": result expected:", expected.result, " actual:", result);
    if (failed) failures++;

    var failed = (visits != expected.visits);
    if (failed || verbose) print(desc, " ", testName, ": visits expected:", expected.visits, " actual:", visits);
    if (failed) failures++;
}

function handleUnexpectedException(e) {
    print(e);
    failures++;
}

function test(replaceValueDesc, replaceValue, expected) {
    var testCase = 0;
    var result;

    visits = 0;
    try {
        result = '12324'.replace('2', replaceValue);
        verify(replaceValueDesc, "'12324'.replace('2', ...)", result, visits, expected[testCase]);
    } catch (e) { handleUnexpectedException(e) }

    visits = 0;
    testCase++;
    try {
        result = '12324'.replace(/2/g, replaceValue);
        verify(replaceValueDesc, "'12324'.replace(/2/g, ...)", result, visits, expected[testCase]);
    } catch (e) { handleUnexpectedException(e) }

    visits = 0;
    testCase++;
    try {
        if (supportsReplaceAll) {
            result = '12324'.replaceAll('2', replaceValue);
            verify(replaceValueDesc, "'12324'.replaceAll('2', ...)", result, visits, expected[testCase]);
        }
    } catch (e) { handleUnexpectedException(e) }

    visits = 0;
    testCase++;
    try {
        result = '12324'.replace('', replaceValue);
        verify(replaceValueDesc, "'12324'.replace('', ...)", result, visits, expected[testCase]);
    } catch (e) { handleUnexpectedException(e) }

    visits = 0;
    testCase++;
    try {
        result = '12324'.replace(/(?:)/g, replaceValue);
        verify(replaceValueDesc, "'12324'.replace(/(?:)/g, ...)", result, visits, expected[testCase]);
    } catch (e) { handleUnexpectedException(e) }

    visits = 0;
    testCase++;
    try {
        if (supportsReplaceAll) {
            result = '12324'.replaceAll('', replaceValue);
            verify(replaceValueDesc, "'12324'.replaceAll('', ...)", result, visits, expected[testCase]);
        }
    } catch (e) { handleUnexpectedException(e) }

    visits = 0;
    testCase++;
    try {
        result = '12324'.replace('0', replaceValue);
        verify(replaceValueDesc, "'12324'.replace('0', ...)", result, visits, expected[testCase]);
    } catch (e) { handleUnexpectedException(e) }

    visits = 0;
    testCase++;
    try {
        result = '12324'.replace(/0/g, replaceValue);
        verify(replaceValueDesc, "'12324'.replace(/0/g, ...)", result, visits, expected[testCase]);
    } catch (e) { handleUnexpectedException(e) }

    visits = 0;
    testCase++;
    try {
        if (supportsReplaceAll) {
            result = '12324'.replaceAll('0', replaceValue);
            verify(replaceValueDesc, "'12324'.replaceAll('0', ...)", result, visits, expected[testCase]);
        }
    } catch (e) { handleUnexpectedException(e) }
}

function foo() {
    visits++;
    return 5;
}

test("function foo", foo, [
    { result: '15324', visits: 1 }, // replace '2'
    { result: '15354', visits: 2 }, // replace /2/g
    { result: '15354', visits: 2 }, // replaceAll '2'
    { result: '512324', visits: 1 }, // replace ''
    { result: '51525352545', visits: 6 }, // replace /(?:)/g
    { result: '51525352545', visits: 6 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

test("function parseInt", parseInt, [
    { result: '1NaN324', visits: 0 }, // replace '2'
    { result: '1NaN324', visits: 0 }, // replace /2/g
    { result: '1NaN324', visits: 0 }, // replaceAll '2'
    { result: 'NaN12324', visits: 0 }, // replace ''
    { result: 'NaN1NaN2NaN3NaN2NaN4NaN', visits: 0 }, // replace /(?:)/g
    { result: 'NaN1NaN2NaN3NaN2NaN4NaN', visits: 0 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

var proxy = new Proxy(foo, {
    apply() {
        visits++;
        return 9;
    }
});

test("proxy", proxy, [
    { result: '19324', visits: 1 }, // replace '2'
    { result: '19394', visits: 2 }, // replace /2/g
    { result: '19394', visits: 2 }, // replaceAll '2'
    { result: '912324', visits: 1 }, // replace ''
    { result: '91929392949', visits: 6 }, // replace /(?:)/g
    { result: '91929392949', visits: 6 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

test("Object", Object, [
    { result: '12324', visits: 0 }, // replace '2'
    { result: '12324', visits: 0 }, // replace /2/g
    { result: '12324', visits: 0 }, // replaceAll '2'
    { result: '12324', visits: 0 }, // replace ''
    { result: '12324', visits: 0 }, // replace /(?:)/g
    { result: '12324', visits: 0 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

var o = {
    toString() {
        visits++;
        return "o";
    }
}

test("o with toString", o, [
    { result: '1o324', visits: 1 }, // replace '2'
    { result: '1o3o4', visits: 1 }, // replace /2/g
    { result: '1o3o4', visits: 1 }, // replaceAll '2'
    { result: 'o12324', visits: 1 }, // replace ''
    { result: 'o1o2o3o2o4o', visits: 1 }, // replace /(?:)/g
    { result: 'o1o2o3o2o4o', visits: 1 }, // replaceAll ''
    { result: '12324', visits: 1 }, // replace '0'
    { result: '12324', visits: 1 }, // replace /0/g
    { result: '12324', visits: 1}, // replaceAll '0'
]);

function bar() {
    visits++;
    return o;
}

test("function bar", bar, [
    { result: '1o324', visits: 2 }, // replace '2'
    { result: '1o3o4', visits: 4 }, // replace /2/g
    { result: '1o3o4', visits: 4 }, // replaceAll '2'
    { result: 'o12324', visits: 2 }, // replace ''
    { result: 'o1o2o3o2o4o', visits: 12 }, // replace /(?:)/g
    { result: 'o1o2o3o2o4o', visits: 12 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

test("undefined", undefined, [
    { result: '1undefined324', visits: 0 }, // replace '2'
    { result: '1undefined3undefined4', visits: 0 }, // replace /2/g
    { result: '1undefined3undefined4', visits: 0 }, // replaceAll '2'
    { result: 'undefined12324', visits: 0 }, // replace ''
    { result: 'undefined1undefined2undefined3undefined2undefined4undefined', visits: 0 }, // replace /(?:)/g
    { result: 'undefined1undefined2undefined3undefined2undefined4undefined', visits: 0 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

test("null", null, [
    { result: '1null324', visits: 0 }, // replace '2'
    { result: '1null3null4', visits: 0 }, // replace /2/g
    { result: '1null3null4', visits: 0 }, // replaceAll '2'
    { result: 'null12324', visits: 0 }, // replace ''
    { result: 'null1null2null3null2null4null', visits: 0 }, // replace /(?:)/g
    { result: 'null1null2null3null2null4null', visits: 0 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

test("0/0", 0/0, [
    { result: '1NaN324', visits: 0 }, // replace '2'
    { result: '1NaN3NaN4', visits: 0 }, // replace /2/g
    { result: '1NaN3NaN4', visits: 0 }, // replaceAll '2'
    { result: 'NaN12324', visits: 0 }, // replace ''
    { result: 'NaN1NaN2NaN3NaN2NaN4NaN', visits: 0 }, // replace /(?:)/g
    { result: 'NaN1NaN2NaN3NaN2NaN4NaN', visits: 0 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

test("-0/0", -0/0, [
    { result: '1NaN324', visits: 0 }, // replace '2'
    { result: '1NaN3NaN4', visits: 0 }, // replace /2/g
    { result: '1NaN3NaN4', visits: 0 }, // replaceAll '2'
    { result: 'NaN12324', visits: 0 }, // replace ''
    { result: 'NaN1NaN2NaN3NaN2NaN4NaN', visits: 0 }, // replace /(?:)/g
    { result: 'NaN1NaN2NaN3NaN2NaN4NaN', visits: 0 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

test("1/0", 1/0, [
    { result: '1Infinity324', visits: 0 }, // replace '2'
    { result: '1Infinity3Infinity4', visits: 0 }, // replace /2/g
    { result: '1Infinity3Infinity4', visits: 0 }, // replaceAll '2'
    { result: 'Infinity12324', visits: 0 }, // replace ''
    { result: 'Infinity1Infinity2Infinity3Infinity2Infinity4Infinity', visits: 0 }, // replace /(?:)/g
    { result: 'Infinity1Infinity2Infinity3Infinity2Infinity4Infinity', visits: 0 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

test("-1/0", -1/0, [
    { result: '1-Infinity324', visits: 0 }, // replace '2'
    { result: '1-Infinity3-Infinity4', visits: 0 }, // replace /2/g
    { result: '1-Infinity3-Infinity4', visits: 0 }, // replaceAll '2'
    { result: '-Infinity12324', visits: 0 }, // replace ''
    { result: '-Infinity1-Infinity2-Infinity3-Infinity2-Infinity4-Infinity', visits: 0 }, // replace /(?:)/g
    { result: '-Infinity1-Infinity2-Infinity3-Infinity2-Infinity4-Infinity', visits: 0 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

test("42", 42, [
    { result: '142324', visits: 0 }, // replace '2'
    { result: '1423424', visits: 0 }, // replace /2/g
    { result: '1423424', visits: 0 }, // replaceAll '2'
    { result: '4212324', visits: 0 }, // replace ''
    { result: '42142242342242442', visits: 0 }, // replace /(?:)/g
    { result: '42142242342242442', visits: 0 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

test("4.2", 4.2, [
    { result: '14.2324', visits: 0 }, // replace '2'
    { result: '14.234.24', visits: 0 }, // replace /2/g
    { result: '14.234.24', visits: 0 }, // replaceAll '2'
    { result: '4.212324', visits: 0 }, // replace ''
    { result: '4.214.224.234.224.244.2', visits: 0 }, // replace /(?:)/g
    { result: '4.214.224.234.224.244.2', visits: 0 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

test("'s'", 's', [
    { result: '1s324', visits: 0 }, // replace '2'
    { result: '1s3s4', visits: 0 }, // replace /2/g
    { result: '1s3s4', visits: 0 }, // replaceAll '2'
    { result: 's12324', visits: 0 }, // replace ''
    { result: 's1s2s3s2s4s', visits: 0 }, // replace /(?:)/g
    { result: 's1s2s3s2s4s', visits: 0 }, // replaceAll ''
    { result: '12324', visits: 0 }, // replace '0'
    { result: '12324', visits: 0 }, // replace /0/g
    { result: '12324', visits: 0 }, // replaceAll '0'
]);

if (failures) {
    print("Found", failures, "failures");
    throw "FAILED";
}
