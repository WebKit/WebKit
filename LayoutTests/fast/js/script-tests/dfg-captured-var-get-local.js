description(
"Tests that GetLocals on captured variables aren't eliminated too aggressively."
);

function foo() {
    var x;
    (function(){
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
        x = 42;
    })();
    var y = x;
    return y;
}

for (var i = 0; i < 100; ++i)
    shouldBe("foo()", "42");
