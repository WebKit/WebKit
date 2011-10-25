description(
"This tests that arguments predicted to be boolean are checked."
);

function predictBooleanArgument(b) {
    if (b) {
        return "yes";
    } else {
        return "no";
    }
}

shouldBe("predictBooleanArgument(true)", "\"yes\"");
shouldBe("predictBooleanArgument(false)", "\"no\"");

for (var i = 0; i < 1000; ++i) {
    predictBooleanArgument(true);
    predictBooleanArgument(false);
}

shouldBe("predictBooleanArgument(true)", "\"yes\"");
shouldBe("predictBooleanArgument(false)", "\"no\"");

shouldBe("predictBooleanArgument(0)", "\"no\"");
shouldBe("predictBooleanArgument(1)", "\"yes\"");
shouldBe("predictBooleanArgument(2)", "\"yes\"");
shouldBe("predictBooleanArgument(3)", "\"yes\"");
shouldBe("predictBooleanArgument(4)", "\"yes\"");

for (var i = 0; i < 1000; ++i) {
    predictBooleanArgument(0);
    predictBooleanArgument(1);
    predictBooleanArgument(2);
    predictBooleanArgument(3);
    predictBooleanArgument(4);
}

shouldBe("predictBooleanArgument(true)", "\"yes\"");
shouldBe("predictBooleanArgument(false)", "\"no\"");

shouldBe("predictBooleanArgument(0)", "\"no\"");
shouldBe("predictBooleanArgument(1)", "\"yes\"");
shouldBe("predictBooleanArgument(2)", "\"yes\"");
shouldBe("predictBooleanArgument(3)", "\"yes\"");
shouldBe("predictBooleanArgument(4)", "\"yes\"");
