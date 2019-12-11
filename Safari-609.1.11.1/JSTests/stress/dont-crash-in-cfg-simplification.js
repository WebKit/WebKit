function bar() {}
noInline(bar);

function baz() { }

function foo() {
    if (typeof baz !== "undefined") {
    } else {
        // The test here is to make sure that we don't merge this basic block
        // with itself. If we did, we'd infinite loop in the compiler and eventually
        // crash due to OOM when growing a Vector.
        while (true) bar();
    }
}
noInline(foo);
for (let i = 0; i < 10000; ++i)
    foo();
