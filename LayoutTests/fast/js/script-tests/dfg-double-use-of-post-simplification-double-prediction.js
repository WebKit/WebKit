description(
"Tests stability of the DFG compiler when you have a double use of a variable that is not revealed to be a double until after CFG simplification."
);

function foo(a) {
    var p = true;
    var x;
    if (p)
        x = 42;
    else
        x = "yo";
    return x + a;
}

for (var i = 0; i < 200; ++i)
    shouldBe("foo(0.5)", "42.5");

