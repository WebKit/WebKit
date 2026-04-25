description(
"This tests that an OSR exit inside of an intrinsic that was not loaded with a method check works correctly."
);

function foo(a,b) {
    return a[0](b.f);
}

for (var i = 0; i < 100; ++i)
    foo([Math.abs], {f:5});

shouldBe("foo([Math.abs], {f:5})", "5");

for (var i = 0; i < 10; ++i)
    shouldBe("foo([Math.abs], {f:5.5})", "5.5");

var successfullyParsed = true;
