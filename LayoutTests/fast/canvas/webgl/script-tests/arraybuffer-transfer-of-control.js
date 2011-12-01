window.jsTestIsAsync = true;

description('Test transfer of control semantics for ArrayBuffers.');
window.testsComplete = 0;

var arraySize = 40;
var arrayOffset = 8;
var arrayEffectiveSize = arraySize - arrayOffset;

var basicBufferTypes =
[
    ["Int32", Int32Array, 4],
    ["Uint32", Uint32Array, 4],
    ["Int8", Int8Array, 1],
    ["Uint8", Uint8Array, 1],
    ["Int16", Int16Array, 2],
    ["Uint16", Uint16Array, 2],
    ["Float32", Float32Array, 4],
    ["Float64", Float64Array, 8]
];

var allBufferTypes =
[
    ["Int32", Int32Array, 4],
    ["Uint32", Uint32Array, 4],
    ["Int8", Int8Array, 1],
    ["Uint8", Uint8Array, 1],
    ["Int16", Int16Array, 2],
    ["Uint16", Uint16Array, 2],
    ["Float32", Float32Array, 4],
    ["Float64", Float64Array, 8],
    ["DataView", DataView, 1]
];

function isTypedArray(view)
{
    for (var i = 0; i < basicBufferTypes.length; ++i) {
        var bufferType = basicBufferTypes[i];
        if (view instanceof bufferType[1]) {
            return true;
        }
    }
    return false;
}

function isDataView(view)
{
    return (view instanceof DataView);
}

function isArrayBuffer(buffer)
{
    return (buffer instanceof ArrayBuffer);
}

function assertBufferClosed(testName, buffer)
{
    if (buffer === null) {
        return true;
    }
    if (!isArrayBuffer(buffer)) {
        testFailed(testName + ": not an array buffer (" + buffer + ")");
        return false;
    }
    if (buffer.byteLength !== 0 || !(buffer.byteLength === 0)) {
        testFailed(testName + ": ArrayBuffer byteLength !== 0");
        return false;
    }
    return true;
}

function assertViewClosed(testName, view)
{
    if (isTypedArray(view) || isDataView(view)) {
        if (view.buffer !== null && !assertBufferClosed(testName, view.buffer))
            return false;
        if (view.byteOffset !== 0 || !(view.byteOffset === 0)) {
            testFailed(testName + ": view byteOffset !== 0");
            return false;
        }
        if (view.byteLength !== 0 || !(view.byteLength === 0)) {
            testFailed(testName + ": view byteLength !== 0");
            return false;
        }
        if (!isDataView(view)) {
            if (view.length !== 0 || !(view.length === 0)) {
                testFailed(testName + ": TypedArray length !== 0");
                return false;
            }
            try {
                var v = view[0];
                if (v !== undefined) {
                    testFailed(testName + ": index get on a closed view did not return undefined.");
                    return false;
                }
            } catch(xn) {
                testFailed(testName + ": index get on a closed view threw an exception: " + xn);
                return false;
            }
            try {
                view[0] = 42;
                var v = view[0];
                if (v !== undefined) {
                    testFailed(testName + ": index set then get on a closed view did not return undefined.");
                    return false;
                }
            } catch(xn) {
                testFailed(testName + ": index set then get on a closed view threw an exception: " + xn);
                return false;
            }
            try {
                view.get(0);
                testFailed(testName + ": get on a closed view succeeded");
                return false;
            } catch (xn) { }
            try {
                view.set(0, 1);
                testFailed(testName + ": set on a closed view succeeded");
                return false;
            } catch (xn) { }
        } else {
            try {
                view.getInt8(0);
                testFailed(testName + ": get on a closed view succeeded");
                return false;
            } catch (xn) { }
            try {
                view.setInt8(0, 1);
                testFailed(testName + ": set on a closed view succeeded");
                return false;
            } catch (xn) { }
        }
    } else {
        testFailed(testName + " not a view (" + view + ")");
        return false;
    }
    return true;
}

function createBuffer(length)
{
    var buffer = new ArrayBuffer(length);
    var view = new Uint8Array(buffer);
    for (var i = 0; i < length; ++i) {
        view[i] = i + 1;
    }
    return buffer;
}

function checkBuffer(testName, buffer, length)
{
    if (!isArrayBuffer(buffer)) {
        testFailed(testName + ": buffer is not an ArrayBuffer");
        return false;
    }
    if (buffer.byteLength !== length) {
        testFailed(testName + ": buffer is the wrong length");
        return false;
    }
    var view = new Uint8Array(buffer);
    for (var i = 0; i < length; ++i) {
        if (view[i] !== i + 1) {
            testFailed(testName + ": buffer contains the wrong data");
            return false;
        }
    }
    return true;
}

function createView(viewType, bytesPerElement)
{
    if (viewType === DataView) {
        var view = new Uint8Array(arraySize);
        for (var i = arrayOffset; i < arraySize; ++i) {
            view[i] = i - arrayOffset + 1;
        }
        return new DataView(view.buffer, arrayOffset, arrayEffectiveSize);
    } else {
        var view = new viewType(new ArrayBuffer(arraySize), arrayOffset, arrayEffectiveSize / bytesPerElement);
        for (var i = 0; i < arrayEffectiveSize / bytesPerElement; ++i) {
            view[i] = i + 1;
        }
        return view;
    }
}

function createEveryView(buffer)
{
    return allBufferTypes.map(function (bufferType) {
        return new bufferType[1](buffer, arrayOffset, arrayEffectiveSize / bufferType[2]);
    });
}

function checkView(testName, typedArrayType, view)
{
    if (!(view instanceof typedArrayType)) {
        testFailed(testName + ": " + view + " not an instance of " + typedArrayType);
        return false;
    }
    if (view.buffer.byteLength !== arraySize ||
        (!(view instanceof DataView) && view.length !== arrayEffectiveSize / view.BYTES_PER_ELEMENT)) {
        testFailed(testName + ": view has the wrong length (" + view.length + ")");
        return false;
    }
    if (view.byteOffset !== arrayOffset) {
        testFailed(testName + ": view has wrong byte offset");
    }
    var max = arrayEffectiveSize;
    if (!(view instanceof DataView)) {
        max = max / view.BYTES_PER_ELEMENT;
    }
    for (var i = 0; i < max; ++i) {
        if (view instanceof DataView) {
            if (view.getInt8(i) !== i + 1) {
                testFailed(testName + ": view contains the wrong data");
                return false;
            }
        } else {
            if (view[i] !== i + 1) {
                testFailed(testName + ": view contains the wrong data");
                return false;
            }
        }
    }
    return true;
}

function checkEmptyArray(testName, array)
{
    if (array === null || array === undefined) {
        testFailed(testName + ": port list is null or undefined");
        return false;
    }
    if (array.length !== 0) {
        testFailed(testName + ": port list is not zero-length");
        return false;
    }
    return true;
}

function wrapSend(testName, message, xfer)
{
    try {
        window.webkitPostMessage(message, xfer, '*');
    } catch (e) {
        testFailed(testName + ": could not webkitPostMessage: " + e);
        doneTest();
        return false;
    }
    return true;
}

function wrapFailSend(testName, message, xfer)
{
    try {
        window.webkitPostMessage(message, xfer, '*');
    } catch (e) {
        return true;
    }
    testFailed(testName + ": expected webkitPostMessage to fail but it didn't.");
    return false;
}

var testList = [{
    name: "sanity check",
    send: function (name) { wrapSend(name, [], []); },
    test: function (name, e) { return true; }
}, {
    name: "raw ArrayBuffer",
    send: function (name) {
        var buffer = createBuffer(3);
        wrapSend(name, buffer, [buffer]);
        assertBufferClosed(name, buffer);
        wrapFailSend(name, buffer, [buffer]);
        wrapFailSend(name, buffer, []);
    },
    test: function (name, e) { return checkBuffer(name, e.data, 3) && checkEmptyArray(name, e.ports); }
}, {
    name: "sending buffers is sane even if cloning doesn't special-case",
    send: function(name) {
        var view = createView(Int32Array, 4);
        var buffer = view.buffer;
        wrapSend(name, [view, buffer], [buffer]);
        assertBufferClosed(name, buffer);
        assertViewClosed(name, view);
    },
    test: function (name, e) {
        if (e.data[0].buffer !== e.data[1]) {
            testFailed("View and buffer were not linked.");
            return false;
        }
        return true;
    }
}, {
    name: "send every view",
    send: function(name) {
        var buffer = createBuffer(arraySize);
        var views = createEveryView(buffer);
        wrapSend(name, views, [buffer]);
        assertBufferClosed(name, buffer);
        wrapFailSend(name, views, [buffer]);
        wrapFailSend(name, views, []);
    },
    test: function (name, e) {
        if (e.data.length !== allBufferTypes.length) {
            testFailed(name + ": not every view was sent.");
        }
        for (var v = 0; v < e.data.length; ++v) {
            var view = e.data[v];
            if (view.buffer !== e.data[0].buffer) {
                testFailed(name + ": not every view pointed to the correct buffer.");
                return false;
            }
        }
        return true;
    }
}, {
    name: "transfer list multiple",
    send: function(name) {
        var buffer = createBuffer(arraySize);
        wrapSend(name, { buffer : buffer }, [buffer,buffer]);
        assertBufferClosed(name, buffer);
        wrapFailSend(name, [buffer], [buffer]);
        wrapFailSend(name, [], [buffer]);
        var buffer2 = createBuffer(arraySize);
        wrapFailSend(name, [], [buffer2, buffer]);
        checkBuffer(name, buffer2, arraySize);
        wrapFailSend(name, [], [buffer, buffer2]);
        checkBuffer(name, buffer2, arraySize);
        wrapFailSend(name, [buffer2], [buffer2, buffer]);
        checkBuffer(name, buffer2, arraySize);
    },
    test: function (name, e) {
        return checkBuffer(name, e.data.buffer, arraySize);
    }
}, {
    name: "transfer neuters unmentioned",
    send: function (name) {
        var buffer = createBuffer(arraySize);
        wrapSend(name, [], [buffer]);
        assertBufferClosed(name, buffer);
    },
    test : function (name, e) {
        return e.data.length == 0;
    }
}];

testList = testList.concat(allBufferTypes.map(function(bufferType) { return {
    name: "raw " + bufferType[0],
    send: function (name) {
        var view = createView(bufferType[1], bufferType[2]);
        wrapSend(name, view, [view.buffer]);
        assertViewClosed(name, view);
        assertBufferClosed(name, view.buffer);
        wrapFailSend(name, view, [view.buffer]);
        wrapFailSend(name, view, []);
    },
    test: function (name, e) {
        return checkView(name, bufferType[1], e.data) && checkEmptyArray(name, e.ports);
    }
}}));

function viewAndBuffer(viewFirst, bufferType) {
    return {
        name: (viewFirst ? "send view, buffer for " : "send buffer, view for ") + bufferType[0],
        send: function (name) {
            var view = createView(bufferType[1], bufferType[2]);
            var buffer = view.buffer;
            wrapSend(name, viewFirst ? [view, buffer] : [buffer, view], [buffer]);
            assertViewClosed(name, view);
            assertBufferClosed(name, buffer);
            wrapFailSend(name, view, [buffer]);
            wrapFailSend(name, view, []);
            wrapFailSend(name, buffer, [buffer]);
            wrapFailSend(name, buffer, []);
            wrapFailSend(name, [view, buffer], [buffer]);
            wrapFailSend(name, [buffer, view], [buffer]);
            wrapFailSend(name, [view, buffer], []);
            wrapFailSend(name, [buffer, view], []);
        },
        test: function (name, e) {
            var view = e.data[viewFirst ? 0 : 1];
            var buffer = e.data[viewFirst ? 1 : 0];
            if (buffer !== view.buffer) {
                testFailed(name + " buffer not shared");
                return false;
            }
            return checkView(name, bufferType[1], view) && checkEmptyArray(name, e.ports);
        }
    }
};

function squashUnrelatedViews(bufferType) {
    return {
        name: "squash unrelated views for " + bufferType[0],
        send: function(name) {
            var view = createView(bufferType[1], bufferType[2]);
            var views = createEveryView(view.buffer);
            var buffer = view.buffer;
            wrapSend(name, view, [view.buffer]);
            assertViewClosed(name, view);
            assertBufferClosed(name, view.buffer);
            for (var v = 0; v < views.length; ++v) {
                assertViewClosed(name + "(view " + v + ")", views[v]);
            }
            wrapFailSend(name, views, [buffer]);
        },
        test: function (name, e) { return checkView(name, bufferType[1], e.data) && checkEmptyArray(name, e.ports); }
    }
}

testList = testList.concat(allBufferTypes.map(function(bufferType) { return viewAndBuffer(true, bufferType); }));
testList = testList.concat(allBufferTypes.map(function(bufferType) { return viewAndBuffer(false, bufferType); }));
testList = testList.concat(allBufferTypes.map(function(bufferType) { return squashUnrelatedViews(bufferType); }));

function doneTest() {
    if (++window.testsComplete == testList.length) {
        finishJSTest();
    }
    else {
        var t = testList[window.testsComplete];
        try {
            t.send(t.name);
        } catch(e) {
            testFailed(t.name + ": on send: " + e);
            doneTest();
        }
    }
}

function windowHandleMessage(event) {
    var currentTest = testList[window.testsComplete];
    if (currentTest.alreadyHit) {
        testFailed(currentTest.name + ": windowHandleMessage hit more than once.");
        return false;
    }
    currentTest.alreadyHit = true;
    try {
        if (currentTest.test(currentTest.name, event)) {
            testPassed(currentTest.name);
        }
    } catch(e) {
        testFailed(currentTest.name + ": on recieve: " + e + ". event.data = " + event.data);
    }
    doneTest();
}

window.addEventListener('message', windowHandleMessage);
window.testsComplete = -1;
doneTest();

successfullyParsed = true;

