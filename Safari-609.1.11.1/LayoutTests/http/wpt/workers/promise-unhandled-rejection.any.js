promise_test(() => {
    return new Promise(resolve => {
        var promise = null;
        self.addEventListener('unhandledrejection', ev => {
            assert_equals(ev.type, "unhandledrejection");
            assert_equals(ev.cancelable, true);
            assert_equals(ev.promise, promise);
            assert_equals(ev.reason, "Reject");
            resolve();
            return false;
        }, false);
        promise = Promise.reject("Reject");
    });
}, "UnhandledRejection event occurs if a rejected promise is not handled.");
