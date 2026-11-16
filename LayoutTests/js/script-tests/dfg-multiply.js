description(
"This tests that the DFG can multiply numbers correctly."
);

function doMultiplyConstant2(a) {
    return a * 2;
}

function doMultiplyConstant3(a) {
    return a * 3;
}

function doMultiplyConstant4(a) {
    return a * 4;
}

// Get it to compile.
for (var i = 0; i < 100; ++i) {
    shouldBe("doMultiplyConstant2(1)", "2");
    shouldBe("doMultiplyConstant2(2)", "4");
    shouldBe("doMultiplyConstant2(4)", "8");
    shouldBe("doMultiplyConstant3(1)", "3");
    shouldBe("doMultiplyConstant3(2)", "6");
    shouldBe("doMultiplyConstant3(4)", "12");
    shouldBe("doMultiplyConstant4(1)", "4");
    shouldBe("doMultiplyConstant4(2)", "8");
    shouldBe("doMultiplyConstant4(4)", "16");
}

// Now do evil.
for (var i = 0; i < 10; ++i) {
    shouldBe("doMultiplyConstant2(1073741824)", "2147483648");
    shouldBe("doMultiplyConstant2(2147483648)", "4294967296");
    shouldBe("doMultiplyConstant3(1073741824)", "3221225472");
    shouldBe("doMultiplyConstant3(2147483648)", "6442450944");
    shouldBe("doMultiplyConstant4(1073741824)", "4294967296");
    shouldBe("doMultiplyConstant4(2147483648)", "8589934592");
    shouldBe("doMultiplyConstant2(-1073741824)", "-2147483648");
    shouldBe("doMultiplyConstant2(-2147483648)", "-4294967296");
    shouldBe("doMultiplyConstant3(-1073741824)", "-3221225472");
    shouldBe("doMultiplyConstant3(-2147483648)", "-6442450944");
    shouldBe("doMultiplyConstant4(-1073741824)", "-4294967296");
    shouldBe("doMultiplyConstant4(-2147483648)", "-8589934592");
}

