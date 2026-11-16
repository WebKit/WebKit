const MockSubprocess = {
    execute: function (command)
    {
        const invocation = {command};
        invocation.promise = new Promise((resolve, reject) => {
            invocation.resolve = resolve;
            invocation.reject = reject;
        });

        if (this._waitingInvocation) {
            this._waitingInvocationResolver();
            this._waitingInvocation = null;
            this._waitingInvocationResolver = null;
        }

        this.invocations.push(invocation);
        return invocation.promise;
    },
    resetAndWaitForInvocation: function ()
    {
        this.reset();
        return this.waitForInvocation();
    },
    waitForInvocation: function ()
    {
        if (!this._waitingInvocation) {
            this._waitingInvocation = new Promise(function (resolve, reject) {
                MockSubprocess._waitingInvocationResolver = resolve;
            });
        }
        return this._waitingInvocation;
    },
    inject: function ()
    {
        const originalSubprocess = global.Subprocess;

        beforeEach(function () {
            MockSubprocess.reset();
            originalSubprocess = global.Subprocess;
            global.Subprocess = MockSubprocess;
        });

        afterEach(function () {
            global.Subprocess = originalSubprocess;
        });

        return  MockSubprocess.invocations ;
    },
    reset: function ()
    {
        MockSubprocess.invocations.length = 0;
        MockSubprocess._waitingInvocation = null;
        MockSubprocess._waitingInvocationResolver = null;
    },

    _waitingInvocation: null,
    _waitingInvocationResolver: null,
    invocations: [],
};

if (typeof module != 'undefined')
    module.exports.MockSubprocess = MockSubprocess;
