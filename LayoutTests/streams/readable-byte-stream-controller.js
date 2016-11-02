'use strict';

if (self.importScripts) {
    self.importScripts('../resources/testharness.js');
}

test(function() {
    const rs = new ReadableStream({
        type: "bytes"
    });
}, "Creating a ReadableStream with an underlyingSource with type property set to 'bytes' should succeed");

const test2 = async_test("Calling read() on a reader associated to a controller that has been errored should be rejected");
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

done();
