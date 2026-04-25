//@ requireOptions("--maxPerThreadStackUsage=400000", "--useTypeProfiler=true", "--exceptionStackTraceLimit=1", "--defaultErrorStackTraceLimit=1")

// This test passes if it does not crash.

var count = 0;

function bar() {
    new foo();
};

function foo() {
    if (count++ > 2000)
        return;
    let proxy = new Proxy({}, {
        set: function() {
            bar();
        }
    });
    try {
        Reflect.set(proxy);
        foo();
    } catch (e) {
    }
}

bar();
