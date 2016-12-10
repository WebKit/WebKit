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

const test2 = async_test("Calling read() on a reader associated to a controller that has been errored should fail with provided error");
test2.step(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });
    const myError = "my error";
    controller.error(myError);

    rs.getReader().read().then(
        test2.step_func(function() {
            assert_unreached('read() should reject on an errored stream');
        }),
        test2.step_func(function(err) {
            assert_equals(myError, err);
            test2.done();
        })
    );
});

const test3 = async_test("Calling read() on a reader associated to a controller that has been closed should not be rejected");
test3.step(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    controller.close();

    rs.getReader().read().then(
        test3.step_func(function(res) {
            assert_object_equals(res, {value: undefined, done: true});
            test3.done();
        }),
        test3.step_func(function(err) {
            assert_unreached("read() should be fulfilled  on a closed stream");
        })
    );
});

const test4 = async_test("Calling read() after a chunk has been enqueued should result in obtaining said chunk");
test4.step(function() {
    let controller;

    const rs = new ReadableStream({
        start: function(c) {
            controller = c;
        },
        type: "bytes"
    });

    controller.enqueue("test");

    rs.getReader().read().then(
        test4.step_func(function(res) {
            assert_object_equals(res, {value: "test", done: false});
            test4.done();
        }),
        test4.step_func(function(err) {
            assert_unreached("read() should be fulfilled after enqueue");
        })
    );
});

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

done();
