function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)} should be ${String(expected)}`);
}
noInline(shouldBe);

function argumentLength() {
    shouldBe(arguments.length, 2);
}
noInline(argumentLength);

function argumentsWithNestedNormalFunction() {
    function nested() {
        var arguments = 0;
    }
    nested();
    shouldBe(arguments.length, 2);
}
noInline(argumentsWithNestedNormalFunction);

function declareLexicalArgumentsWithBlock() {
    {
        let arguments = 1;
    }
    shouldBe(arguments.length, 3);
}
noInline(declareLexicalArgumentsWithBlock);

function forInLetArguments() {
    for (let arguments in [1, 2]) {
    }
    shouldBe(arguments.length, 2);
}
noInline(forInLetArguments);

function forOfLetArguments() {
    for (let arguments of [1, 2]) {
    }
    shouldBe(arguments.length, 0);
}
noInline(forOfLetArguments);

for (let i = 0; i < 1e4; i++) {
    argumentLength(1, 2);
    argumentsWithNestedNormalFunction(1, 2);
    declareLexicalArgumentsWithBlock(1,2,3);
    forInLetArguments(1, 2);
    forOfLetArguments();
}
