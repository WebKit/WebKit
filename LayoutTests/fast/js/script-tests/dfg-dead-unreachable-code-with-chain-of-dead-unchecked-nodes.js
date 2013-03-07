description(
"Tests that code that is dead, unreachable, and contains a chain of nodes that use each other in an untyped way doesn't result in the IR getting corrupted."
);

function foo(a) {
    function bar(p) {
        if (p) {
            var x = a; // It's dead and unreachable, and it involves a GetScopeVar(GetScopeRegisters(GetMyScope())).
        }
        return 5;
    }
    return bar;
}

// Since the function contains unreachable code we need a higher optimization threshold.
for (var i = 0; i < 1000; ++i)
    shouldBe("foo(42)(false)", "5");
