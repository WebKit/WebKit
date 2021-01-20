promise_test(() => {
    var counter = 0;
    self.queueMicrotask(() => {
        assert_equals(counter++, 1);
        self.queueMicrotask(() => {
            assert_equals(counter++, 2);
        });
    });
    var promise = new Promise((resolve, reject) => {
        setTimeout(() => {
            assert_equals(counter++, 3);
            resolve();
        }, 0);
    });
    assert_equals(counter++, 0);
    return promise;
}, `Queued microtasks should be drained before executing macrotasks`);

promise_test(() => {
    return new Promise((resolve, reject) => {
        self.queueMicrotask(function () {
            try {
                assert_equals(arguments.length, 0);
                assert_equals(this, self);
                self.queueMicrotask(function () {
                    try {
                        assert_equals(this, self);
                        self.queueMicrotask(function () {
                            'use strict';
                            try {
                                assert_equals(this, undefined);
                                resolve();
                            } catch (e) {
                                reject(e);
                            }
                        });
                    } catch (e) {
                        reject(e);
                    }
                });
            } catch (e) {
                reject(e);
            }
        });
    });
}, `queueMicrotask's callback has zero arguments and self as |this|`);

promise_test(() => {
    return new Promise((resolve ,reject) => {
        var counter = 0;
        Promise.resolve().then(() => {
            assert_equals(counter++, 1);
            self.queueMicrotask(() => {
                assert_equals(counter++, 3);
                resolve();
            });
        });
        self.queueMicrotask(() => {
            assert_equals(counter++, 2);
        });
        assert_equals(counter++, 0);
    });
}, `queueMicrotask and Promise uses the same Microtask queue`);

test(() => {
    assert_throws_js(TypeError, () => {
        self.queueMicrotask();
    });
    assert_throws_js(TypeError, () => {
        self.queueMicrotask(null);
    });
    assert_throws_js(TypeError, () => {
        self.queueMicrotask(undefined);
    });
    assert_throws_js(TypeError, () => {
        self.queueMicrotask(42);
    });
    assert_throws_js(TypeError, () => {
        self.queueMicrotask("42");
    });
    assert_throws_js(TypeError, () => {
        self.queueMicrotask(true);
    });
    assert_throws_js(TypeError, () => {
        self.queueMicrotask(Symbol("42"));
    });
    assert_throws_js(TypeError, () => {
        self.queueMicrotask({});
    });
    assert_throws_js(TypeError, () => {
        self.queueMicrotask({ handleEvent() { } });
    });
}, `queueMicrotask should reject non-function arguments`);
