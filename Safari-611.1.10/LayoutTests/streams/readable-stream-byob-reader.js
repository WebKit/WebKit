'use strict';

if (self.importScripts) {
    self.importScripts('../resources/testharness.js');
}

test(function() {
    const rs = new ReadableStream({
        type: "bytes"
    });
    rs.getReader({ mode: 'byob' });
}, "Getting a ReadableStreamBYOBReader should succeed");

test(function() {
    const rs = new ReadableStream();
    const tmp = 12;
    assert_throws(new TypeError("Can only call ReadableStream.getReader on instances of ReadableStream"),
        function() { rs.getReader.apply(tmp); });
}, "Calling getReader() with a this object different from ReadableStream should throw a TypeError");

test(function() {
    const rs = new ReadableStream();

    assert_throws(new TypeError("ReadableStreamBYOBReader needs a ReadableByteStreamController"),
        function() { rs.getReader({ mode: 'byob' }); });
}, "Calling getReader({ mode: 'byob' }) with a ReadableStream whose controller is a ReadableStreamDefaultController should throw a TypeError");

promise_test(function(test) {
    const rs = new ReadableStream({ type: 'bytes' });
    const reader = rs.getReader({ mode: 'byob' });
    let rp = reader.cancel.apply(rs);
    const myError= new TypeError("Can only call ReadableStreamBYOBReader.cancel() on instances of ReadableStreamBYOBReader");

    return promise_rejects(test, myError, rp);
}, "Calling ReadableStreamBYOBReader.cancel() with a this object different from ReadableStreamBYOBReader should be rejected");

promise_test(function(test) {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    const reader = rs.getReader({ mode: 'byob' });
    const myError = new TypeError("Sample error");
    controller.error(myError);
    let rp = reader.cancel("Sample reason");

    return promise_rejects(test, myError, rp);
}, "Calling ReadableStreamBYOBReader.cancel() on a ReadableStream that has been errored should result in a promise rejected with the same error");

promise_test(function(test) {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    const reader = rs.getReader({ mode: 'byob' });
    controller.close();

    return reader.cancel("Sample reason").then(
        function(res) {
            assert_object_equals(res, undefined);
        }
    );
}, "Calling ReadableStreamBYOBReader.cancel() on a ReadableStream that has been closed should result in a promise resolved with undefined");

promise_test(function(test) {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    const reader = rs.getReader({ mode: 'byob' });
    controller.close();

    return reader.closed.then(
        function(res) {
            assert_object_equals(res, undefined);
        }
    );
}, "If controller is closed after ReadableStreamBYOBReader creation, ReadableStreamBYOBReader.closed should be a promise resolved with undefined");

promise_test(function(test) {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    controller.close();
    const reader = rs.getReader({ mode: 'byob' });

    return reader.closed.then(
        function(res) {
            assert_object_equals(res, undefined);
        }
    );
}, "If controller has already been closed when ReadableStreamBYOBReader is created, ReadableStreamBYOBReader.closed should be a promise resolved with undefined");

promise_test(function(test) {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    const reader = rs.getReader({ mode: 'byob' });
    const myError = new TypeError("Sample error");
    controller.error(myError);

    return promise_rejects(test, myError, reader.closed);
}, "If controller is errored after ReadableStreamBYOBReader creation, ReadableStreamBYOBReader.closed should be a promise rejected with the same error");

promise_test(function(test) {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    const myError = new TypeError("Sample error");
    controller.error(myError);
    const reader = rs.getReader({ mode: 'byob' });

    return promise_rejects(test, myError, reader.closed);
}, "If controller has already been errored when ReadableStreamBYOBReader is created, ReadableStreamBYOBReader.closed should be a promise rejected with the same error");

test(function() {
    const rs = new ReadableStream({ type: 'bytes' });
    const reader = rs.getReader({ mode: 'byob' });
    assert_throws(new TypeError("Can only call ReadableStreamBYOBReader.releaseLock() on instances of ReadableStreamBYOBReader"),
        function() { reader.releaseLock.apply(rs); });
}, "Calling ReadableStreamBYOBReader.releaseLock() with a this object different from ReadableStreamBYOBReader should be rejected");

promise_test(function(test) {
    const rs = new ReadableStream({
        type: "bytes"
    });

    const reader = rs.getReader({ mode: 'byob' });
    reader.releaseLock();
    const myError = new TypeError();

    return promise_rejects(test, myError, reader.closed);
}, "Calling ReadableStreamBYOBReader.releaseLock() on a stream that is readable should result in ReadableStreamBYOBReader.closed promise to be rejected with a TypeError");

promise_test(function(test) {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    const reader = rs.getReader({ mode: 'byob' });
    controller.close();
    reader.releaseLock();
    const myError = new TypeError();

    return promise_rejects(test, myError, reader.closed);
}, "Calling ReadableStreamBYOBReader.releaseLock() on a stream that is not readable should result in ReadableStreamBYOBReader.closed promise to be rejected with a TypeError");

promise_test(function(test) {
    const rs = new ReadableStream({ type: 'bytes' });
    const reader = rs.getReader({ mode: 'byob' });
    let rp = reader.read.apply(rs);
    const myError= new TypeError("Can only call ReadableStreamBYOBReader.read() on instances of ReadableStreamBYOBReader");

    return promise_rejects(test, myError, rp);
}, "Calling ReadableStreamBYOBReader.read() with a this object different from ReadableStreamBYOBReader should be rejected");

done();
