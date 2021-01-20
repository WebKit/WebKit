//@ requireOptions("--jitPolicyScale=0")

function foo(x) {
    try {
        new x();
    } catch {
    }
}

foo(function() {});
for (let i = 0; i < 10000; ++i)
    foo(() => undefined);
