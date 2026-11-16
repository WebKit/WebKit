promise_test(() => {
    var counter = 0;
    var promise = new Promise(function (resolve) {
        setTimeout(function () {
            assert_equals(counter++, 3);
            resolve();
        }, 0);
        Promise.resolve().then(function () {
            assert_equals(counter++, 2);
        });
        assert_equals(counter++, 0);
    });
    assert_equals(counter++, 1);
    return promise;
}, "Microtasks should be executed earlier than macrotasks.");

promise_test(() => {
    var counter = 0;
    var promise = new Promise(function (resolve) {
        setTimeout(function () {
            assert_equals(counter++, 4);
            resolve();
        }, 0);
        Promise.resolve().then(function () {
            Promise.resolve().then(function () {
                assert_equals(counter++, 3);
            });
            assert_equals(counter++, 2);
        });
        assert_equals(counter++, 0);
    });
    assert_equals(counter++, 1);
    return promise;
}, "Nested microtasks should be executed earlier than macrotasks.");

promise_test(() => {
    var counter = 0;
    return new Promise(function (resolve) {
        Promise.resolve().then(function () {
            assert_equals(counter++, 1);
        });
        Promise.resolve().then(function () {
            assert_equals(counter++, 2);
        });
        Promise.resolve().then(function () {
            assert_equals(counter++, 3);
            resolve();
        });
        assert_equals(counter++, 0);
    });
}, "Microtasks should be executed in queued order.");
