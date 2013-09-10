description(
"Tests that the DFG's support for ResolveGlobal works when the structure is not a dictionary but the resolve has gone polymorphic."
)

function foo() {
    return x;
}

x = 42;

for (var i = 0; i < 1000; ++i) {
    eval("i" + i + " = function() { }; i" + i + ".prototype = this; (function(){ var o = new i" + i + "(); var result = 0; for (var j = 0; j < 100; ++j) result += o.x; return result; })()");
    for (var j = 0; j < 2; ++j)
        shouldBe("foo()", "42");
}
