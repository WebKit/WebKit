description(
"Checks that if a DFG AllocationProfileWatchpoint fires and the callee is otherwise dead, we still preserve the callee for the bytecode and don't crash."
);

function Foo() {
    this.f = 42;
}

function foo() {
    eval("// Don't optimize me!");
    return new Foo().f;
}

for (var i = 0; i < 100; ++i) {
    if (i == 95)
        Foo.prototype = {foo: 62};
    shouldBe("foo()", "42");
}
