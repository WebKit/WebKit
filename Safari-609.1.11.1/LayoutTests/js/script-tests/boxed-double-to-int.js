description(
"This tests that converting a boxed double to an integer does not crash the register allocator."
);

function boxedDoubleToInt(x, y) {
    var y = x / 2;
    var z = y + 2;
    return (y | 1) + z;
}

shouldBe("boxedDoubleToInt(1, 2)", "3.5");
shouldBe("boxedDoubleToInt(3, 4)", "4.5");
shouldBe("boxedDoubleToInt(5, 6)", "7.5");
shouldBe("boxedDoubleToInt(7, 8)", "8.5");
shouldBe("boxedDoubleToInt(9, 10)", "11.5");
