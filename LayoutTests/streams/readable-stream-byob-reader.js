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

test(() => {
    const methods = ['cancel', 'constructor', 'read', 'releaseLock'];
    const properties = methods.concat(['closed']).sort();

    const rs = new ReadableStream({ type: "bytes" });
    const reader = rs.getReader({ mode: 'byob' });

    const proto = Object.getPrototypeOf(reader);

    assert_array_equals(Object.getOwnPropertyNames(proto).sort(), properties);

    for (const m of methods) {
        const propDesc = Object.getOwnPropertyDescriptor(proto, m);
        assert_equals(propDesc.enumerable, false, 'method should be non-enumerable');
        assert_equals(propDesc.configurable, true, 'method should be configurable');
        assert_equals(propDesc.writable, true, 'method should be writable');
        assert_equals(typeof reader[m], 'function', 'should have be a method');
    }

    const closedPropDesc = Object.getOwnPropertyDescriptor(proto, 'closed');
    assert_equals(closedPropDesc.enumerable, false, 'closed should be non-enumerable');
    assert_equals(closedPropDesc.configurable, true, 'closed should be configurable');
    assert_not_equals(closedPropDesc.get, undefined, 'closed should have a getter');
    assert_equals(closedPropDesc.set, undefined, 'closed should not have a setter');

    assert_equals(reader.cancel.length, 1, 'cancel has 1 parameter');
    assert_equals(reader.constructor.length, 1, 'constructor has 1 parameter');
    assert_equals(reader.read.length, 1, 'read has 1 parameter');
    assert_equals(reader.releaseLock.length, 0, 'releaseLock has 0 parameter');

}, 'ReadableStreamBYOBReader instances should have the correct list of properties');

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

done();
