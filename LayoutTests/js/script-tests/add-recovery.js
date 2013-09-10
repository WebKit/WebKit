description(
"This tests that speculation recovery of destructive additions on unboxed integers works."
);

function destructiveAddForBoxedInteger(a,b,c) {
    var a_ = a.x;
    var d = a_ + b;
    return c + d + b;
}

// warm-up foo to be integer
for (var i = 0; i < 100; ++i) {
    destructiveAddForBoxedInteger({x:1}, 2, 3);
}

shouldBe("destructiveAddForBoxedInteger({x:1}, 2, 4)", "9");
shouldBe("destructiveAddForBoxedInteger({x:2147483647}, 2, 4)", "2147483655");
shouldBe("destructiveAddForBoxedInteger({x:2}, 2147483647, 4)", "4294967300");
shouldBe("destructiveAddForBoxedInteger({x:2147483647}, 2147483647, 4)", "6442450945");
shouldBe("destructiveAddForBoxedInteger({x:1}, 2, 2147483647)", "2147483652");
