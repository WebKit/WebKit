'use strict';

if (self.importScripts) {
    self.importScripts('../resources/testharness.js');
}

test(function() {
    const rs = new ReadableStream({
        type: "bytes"
    });
}, "Creating a ReadableStream with an underlyingSource with type property set to 'bytes' should succeed");

test(() => {
    const methods = ['close', 'constructor', 'enqueue', 'error'];
    // FIXME: Add byobRequest when implemented.
    const properties = methods.concat(['desiredSize']).sort();

    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    const proto = Object.getPrototypeOf(controller);

    assert_array_equals(Object.getOwnPropertyNames(proto).sort(), properties);

    for (const m of methods) {
        const propDesc = Object.getOwnPropertyDescriptor(proto, m);
        assert_equals(propDesc.enumerable, false, 'method should be non-enumerable');
        assert_equals(propDesc.configurable, true, 'method should be configurable');
        assert_equals(propDesc.writable, true, 'method should be writable');
        assert_equals(typeof controller[m], 'function', 'should have be a method');
    }

    const desiredSizePropDesc = Object.getOwnPropertyDescriptor(proto, 'desiredSize');
    assert_equals(desiredSizePropDesc.enumerable, false, 'desiredSize should be non-enumerable');
    assert_equals(desiredSizePropDesc.configurable, true, 'desiredSize should be configurable');
    assert_not_equals(desiredSizePropDesc.get, undefined, 'desiredSize should have a getter');
    assert_equals(desiredSizePropDesc.set, undefined, 'desiredSize should not have a setter');

    assert_equals(controller.close.length, 0, 'close has 0 parameter');
    assert_equals(controller.constructor.length, 3, 'constructor has 3 parameters');
    assert_equals(controller.enqueue.length, 1, 'enqueue has 1 parameter');
    assert_equals(controller.error.length, 1, 'error has 1 parameter');

}, 'ReadableByteStreamController instances should have the correct list of properties');

test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    assert_throws(new TypeError("Can only call ReadableByteStreamController.error on instances of ReadableByteStreamController"),
        function() { controller.error.apply(rs); });
}, "Calling error() with a this object different from ReadableByteStreamController should throw a TypeError");

test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    assert_throws(new TypeError("Can only call ReadableByteStreamController.close on instances of ReadableByteStreamController"),
        function() { controller.close.apply(rs); });
}, "Calling close() with a this object different from ReadableByteStreamController should throw a TypeError");

test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    assert_throws(new TypeError("Can only call ReadableByteStreamController.enqueue on instances of ReadableByteStreamController"),
        function() { controller.enqueue.apply(rs, new Int8Array(1)); });
}, "Calling enqueue() with a this object different from ReadableByteStreamController should throw a TypeError");

test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    controller.enqueue(new Int8Array(2));
    controller.close();

    assert_throws(new TypeError("ReadableByteStreamController is requested to close"),
        function() { controller.enqueue(new Int8Array(1)); });
}, "Calling enqueue() when close has been requested but not yet performed should throw a TypeError");

test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });
    controller.close();

    assert_throws(new TypeError("ReadableStream is not readable"),
        function() {
            controller.enqueue(new Int8Array(1));
        });
}, "Calling enqueue() when stream is not readable should throw a TypeError");

test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    const invalidChunk = function() {};

    assert_throws(new TypeError("Provided chunk is not an object"),
        function() { controller.enqueue(invalidChunk); });
}, "Calling enqueue() with a chunk that is not an object should trhow a TypeError");

test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    const invalidChunk = {};

    assert_throws(new TypeError("Provided chunk is not an object"),
        function() { controller.enqueue(invalidChunk); });
}, "Calling enqueue() with a chunk that is not an ArrayBufferView should throw a TypeError");

test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    assert_throws(new TypeError("ReadableStream is not readable"),
        function() {
            controller.close();
            controller.error();
        });
}, "Calling error() after calling close() should throw a TypeError");

test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    assert_throws(new TypeError("ReadableStream is not readable"),
        function() {
            controller.error();
            controller.error();
        });
}, "Calling error() after calling error() should throw a TypeError");

test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    assert_throws(new TypeError("Close has already been requested"),
        function() {
            controller.close();
            controller.close();
        });
}, "Calling close() after calling close() should throw a TypeError");

test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    assert_throws(new TypeError("ReadableStream is not readable"),
        function() {
            controller.error();
            controller.close();
        });
}, "Calling close() after calling error() should throw a TypeError");

promise_test(function(test) {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });
    const myError = new Error("my error");
    controller.error(myError);

    return promise_rejects(test, myError, rs.getReader().read());
}, "Calling read() on a reader associated to a controller that has been errored should fail with provided error");

promise_test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    controller.close();

    return rs.getReader().read().then(
        function(res) {
            assert_object_equals(res, {value: undefined, done: true});
        }
    );
}, "Calling read() on a reader associated to a controller that has been closed should not be rejected");

promise_test(function(test) {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    const myError = new Error("My error");
    let readingPromise = rs.getReader().read();
    controller.error(myError);

    return promise_rejects(test, myError, readingPromise);
}, "Pending reading promise should be rejected if controller is errored (case where autoAllocateChunkSize is undefined)");

promise_test(function(test) {
    let controller;

    const rs = new ReadableStream({
        autoAllocateChunkSize: 128,
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    const myError = new Error("My error");
    let readingPromise = rs.getReader().read();
    controller.error(myError);

    return promise_rejects(test, myError, readingPromise);
}, "Pending reading promise should be rejected if controller is errored (case where autoAllocateChunkSize is specified)");

promise_test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    const buffer = new Uint8Array([3]);
    controller.enqueue(buffer);

    return rs.getReader().read().then(
        function(res) {
            assert_object_equals(res, {value: buffer, done: false});
        }
    );
}, "Enqueuing a chunk, getting a reader and calling read should result in a promise resolved with said chunk");

promise_test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    let promise = rs.getReader().read();
    const buffer = new Uint8Array([1]);
    controller.enqueue(buffer);
    return promise.then(
        function(res) {
            assert_object_equals(res, {value: buffer, done: false});
        }
    );
}, "Getting a reader, calling read and enqueuing a chunk should result in the read promise being resolved with said chunk");

promise_test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    const reader = rs.getReader();
    const buffer = new Uint8Array([1]);
    controller.enqueue(buffer);
    return reader.read().then(
        function(res) {
            assert_object_equals(res, {value: buffer, done: false});
        }
    );
}, "Getting a reader, enqueuing a chunk and finally calling read should result in a promise resolved with said chunk");

test(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    assert_equals(controller.desiredSize, 1, "by default initial value of desiredSize should be 1");
}, "By default initial value of desiredSize should be 1");

promise_test(function() {
    const rs = new ReadableStream({
        type: "bytes"
    });

    return rs.cancel().then(
        function(res) {
            assert_object_equals(res, undefined);
        }
    );
}, "Calling cancel() on a readable ReadableStream that is not locked to a reader should return a promise whose fulfillment handler returns undefined");

promise_test(function() {
    let pullCalls = 0;
    const rs = new ReadableStream({
        pull: function () {
            pullCalls++;
        },
        type: "bytes"
    });
    return new Promise(function(resolve, reject) {
        setTimeout(function() {
            if (pullCalls === 1)
                resolve("ok");
            else
                reject("1 call should have been made to pull function");
        }, 200);
    });
}, "Test that pull is called once when a new ReadableStream is created with a ReadableByteStreamController");

promise_test(function() {
    const myError = new Error("Pull failed");
    const rs = new ReadableStream({
        pull: function () {
            throw myError;
        },
        type: "bytes"
    });

    return new Promise(function(resolve, reject) {
        setTimeout(function() {
            rs.cancel().then(
                function (res) { reject("Cancel should return a promise resolved with rejection"); },
                function (err) {
                    if (err === myError)
                        resolve();
                    else
                        reject("Reason for rejection should be the error that was thrown in pull");
                }
            )
        }, 200)});
}, "Calling cancel after pull has thrown an error should result in a promise rejected with the same error");

promise_test(function() {
    const myError = new Error("Start failed");
    const rs = new ReadableStream({
        start: function () {
            return new Promise(function(resolve, reject) { reject(myError); });
        },
        type: "bytes"
    });

    return new Promise(function(resolve, reject) {
        setTimeout(function() {
            rs.cancel().then(
                function (res) { reject("An error should have been thrown"); },
                function (err) {
                    if (err === myError)
                        resolve();
                    else
                        reject("Reason for rejection should be the error that led the promise returned by start to fail");
                }
            )
        }, 200)});
}, "Calling cancel after creating a ReadableStream with an underlyingByteStream's start function returning a rejected promise should result in a promise rejected with the same error");

done();
