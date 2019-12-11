description(
"Tests that we do ToString conversions correctly when String.prototype.valueOf is not what we wanted."
);

String.prototype.valueOf = function() { return 42; }

function foo(a) {
    for (var i = 0; i < 100; ++i)
        a = new String(a);
    return a;
}

for (var i = 0; i < 100; ++i)
    shouldBe("\"\" + foo(\"hello\")", "\"42\"");
