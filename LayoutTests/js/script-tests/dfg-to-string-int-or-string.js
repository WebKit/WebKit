description(
"Tests that we do ToString conversions correctly when String.prototype.valueOf is not what we wanted."
);

function foo(a) {
    for (var i = 0; i < 100; ++i)
        a = new String(a);
    return a;
}

for (var i = 0; i < 1000; ++i)
    shouldBe("\"\" + foo(i % 2 ? 42 : \"hello\")", "i % 2 ? \"42\" : \"hello\"");
