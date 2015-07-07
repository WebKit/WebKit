window.jsTestIsAsync = true;

description('Test passing ArrayBuffers and ArrayBufferViews in messages.');
window.testsComplete = 0;

function classCompare(testName, got, sent) {
    var classString = Object.prototype.toString;
    var gotClass = classString.call(got);
    var sentClass = classString.call(sent);
    if (gotClass !== sentClass) {
        testFailed(testName + ": class " + sentClass + " became " + gotClass);
        return false;
    } else {
        testPassed(testName + ": classes are " + sentClass);
        return true;
    }
}

function bufferCompare(testName, got, sent) {
    if (!classCompare(testName, got, sent)) {
        return false;
    }
    if (got.byteLength !== sent.byteLength) {
        testFailed(testName + ": expected byteLength " + sent.byteLength + " bytes, got " + got.byteLength);
        return false;
    } else {
        testPassed(testName + ": buffer lengths are " + sent.byteLength);
    }
    var gotReader = new Uint8Array(got);
    var sentReader = new Uint8Array(sent);
    for (var i = 0; i < sent.byteLength; ++i) {
        if (gotReader[i] !== sentReader[i]) {
            testFailed(testName + ": buffers differ starting at byte " + i);
            return false;
        }
    }
    testPassed(testName + ": buffers have the same contents");
    return true;
}

function viewCompare(testName, got, sent) {
    if (!classCompare(testName, got, sent)) {
        return false;
    }
    if (!bufferCompare(testName, got.buffer, sent.buffer)) {
        return false;
    }
    if (got.byteOffset !== sent.byteOffset) {
        testFailed(testName + ": offset " + sent.byteOffset + " became " + got.byteOffset);
        return false;
    } else {
        testPassed(testName + ": offset is " + sent.byteOffset);
    }
    if (got.byteLength !== sent.byteLength) {
        testFailed(testName + ": length " + sent.byteLength + " became " + got.byteLength);
        return false;
    } else {
        testPassed(testName + ": length is " + sent.byteLength);
    }
    return true;
}

function typedArrayCompare(testName, got, sent) {
    if (!viewCompare(testName, got, sent)) {
        return false;
    }
    if (got.BYTES_PER_ELEMENT !== sent.BYTES_PER_ELEMENT) {
        // Sanity checking.
        testFailed(testName + ": expected BYTES_PER_ELEMENT " + sent.BYTES_PER_ELEMENT + ", saw " + got.BYTES_PER_ELEMENT);
        return false;
    }
    return true;
}

function dataViewCompare(testName, got, sent) {
    return viewCompare(testName, got, sent);
}

function dataViewCompare2(testName, got, sent) {
    for (var i = 0; i < 2; ++i) {
        if (!dataViewCompare(testName, got[i], sent[i])) {
            return false;
        }
    }
    if (got[0].buffer !== got[1].buffer) {
        testFailed(testName + ": expected the same ArrayBuffer for both views");
        return false;
    }
    return true;
}
function dataViewCompare3(testName, got, sent) {
    for (var i = 0; i < 3; i += 2) {
        if (!dataViewCompare(testName, got[i], sent[i])) {
            return false;
        }
    }
    if (got[1].x !== sent[1].x || got[1].y !== sent[1].y) {
        testFailed(testName + ": {x:1, y:1} was not transferred properly");
        return false;
    }
    if (got[0].buffer !== got[2].buffer) {
        testFailed(testName + ": expected the same ArrayBuffer for both views");
        return false;
    }
    return false;
}


function createBuffer(length) {
    var buffer = new ArrayBuffer(length);
    var view = new Uint8Array(buffer);
    for (var i = 0; i < length; ++i) {
        view[i] = i + 1;
    }
    return buffer;
}

function createTypedArray(typedArrayType, length) {
    var view = new typedArrayType(length);
    for (var i = 0; i < length; ++i) {
        view[i] = i + 1;
    }
    return view;
}

function createTypedArrayOverBuffer(typedArrayType, typedArrayElementSize, length, subStart, subLength) {
    var buffer = new ArrayBuffer(length * typedArrayElementSize);
    if (subStart === undefined) {
        subStart = 0;
        subLength = length;
    }
    return new typedArrayType(buffer, subStart * typedArrayElementSize, subLength);
}

var basicBufferTypes = [
    ["Int32", Int32Array, 4],
    ["Uint32", Uint32Array, 4],
    ["Int8", Int8Array, 1],
    ["Uint8", Uint8Array, 1],
    ["Uint8Clamped", Uint8ClampedArray, 1],
    ["Int16", Int16Array, 2],
    ["Uint16", Uint16Array, 2],
    ["Float32", Float32Array, 4],
    ["Float64", Float64Array, 8]
];

var arrayBuffer1 = createBuffer(1);

var testList = [
    ['ArrayBuffer0', new ArrayBuffer(0), bufferCompare],
    ['ArrayBuffer1', createBuffer(1), bufferCompare],
    ['ArrayBuffer128', createBuffer(128), bufferCompare],
    ['DataView0', new DataView(new ArrayBuffer(0)), dataViewCompare],
    ['DataView1', new DataView(createBuffer(1)), dataViewCompare],
    ['DataView1-dup', [new DataView(arrayBuffer1), new DataView(arrayBuffer1)], dataViewCompare2],
    ['DataView1-dup2', [new DataView(arrayBuffer1), {x:1, y:1}, new DataView(arrayBuffer1)], dataViewCompare3],
    ['DataView128', new DataView(createBuffer(128)), dataViewCompare],
    ['DataView1_offset_at_end', new DataView(createBuffer(1), 1, 0), dataViewCompare],
    ['DataView128_offset_at_end', new DataView(createBuffer(128), 128, 0), dataViewCompare],
    ['DataView128_offset_slice_length_0', new DataView(createBuffer(128), 64, 0), dataViewCompare],
    ['DataView128_offset_slice_length_1', new DataView(createBuffer(128), 64, 1), dataViewCompare],
    ['DataView128_offset_slice_length_16', new DataView(createBuffer(128), 64, 16), dataViewCompare],
    ['DataView128_offset_slice_unaligned', new DataView(createBuffer(128), 63, 15), dataViewCompare]
];

testList = testList.concat(basicBufferTypes.map(function(t)
    {return [t[0] + "_0", createTypedArray(t[1], 0), typedArrayCompare];}));
testList = testList.concat(basicBufferTypes.map(function(t)
    {return [t[0] + "_1", createTypedArray(t[1], 1), typedArrayCompare];}));
testList = testList.concat(basicBufferTypes.map(function(t)
    {return [t[0] + "_128", createTypedArray(t[1], 128), typedArrayCompare];}));
testList = testList.concat(basicBufferTypes.map(function(t)
    {return [t[0] + "_0_buffer", createTypedArrayOverBuffer(t[1], t[2], 0), typedArrayCompare];}));
testList = testList.concat(basicBufferTypes.map(function(t)
    {return [t[0] + "_1_buffer", createTypedArrayOverBuffer(t[1], t[2], 1), typedArrayCompare];}));
testList = testList.concat(basicBufferTypes.map(function(t)
    {return [t[0] + "_128_buffer", createTypedArrayOverBuffer(t[1], t[2], 128), typedArrayCompare];}));
testList = testList.concat(basicBufferTypes.map(function(t)
    {return [t[0] + "_1_buffer_offset_at_end", createTypedArrayOverBuffer(t[1], t[2], 1, 1, 0), typedArrayCompare];}));
testList = testList.concat(basicBufferTypes.map(function(t)
    {return [t[0] + "_128_buffer_offset_at_end", createTypedArrayOverBuffer(t[1], t[2], 128, 128, 0), typedArrayCompare];}));
testList = testList.concat(basicBufferTypes.map(function(t)
    {return [t[0] + "_128_buffer_offset_slice_length_0", createTypedArrayOverBuffer(t[1], t[2], 128, 64, 0), typedArrayCompare];}));
testList = testList.concat(basicBufferTypes.map(function(t)
    {return [t[0] + "_128_buffer_offset_slice_length_1", createTypedArrayOverBuffer(t[1], t[2], 128, 64, 1), typedArrayCompare];}));
testList = testList.concat(basicBufferTypes.map(function(t)
    {return [t[0] + "_128_buffer_offset_slice_length_16", createTypedArrayOverBuffer(t[1], t[2], 128, 64, 16), typedArrayCompare];}));

function doneTest() {
    if (++window.testsComplete == testList.length) {
        finishJSTest();
    }
}

function windowHandleMessage(e) {
    var currentTest = testList[e.data.testNum];
    var expectedResult = currentTest[1];
    try {
        currentTest[2](currentTest[0], e.data.testData, expectedResult);
    } catch(e) {
        testFailed(currentTest[0] + ": unexpected exception " + e);
    }
    doneTest();
}
window.addEventListener('message', windowHandleMessage);

for (var t = 0; t < testList.length; ++t) {
    var currentTest = testList[t];
    var message = {testNum: t, testData: currentTest[1]};
    window.postMessage(message, '*');
}
