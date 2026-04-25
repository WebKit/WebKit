
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


function inner(value) {
    switch (value + "Statement") {
    case "ExpressionStatement":
        return 0;
    case "BreakStatement":
        return 1;
    case "ThrowStatement":
        return 2;
    case "IfStatement":
        return 3;
    case "WhileStatement":
        return 4;
    case "DoWhileStatement":
        return 5;
    case "ForStatement":
        return 6;
    default:
        return 7;
    }
}

function outer(value) {
    switch (value) {
    case "Expression":
        return 0 + inner(value);
    case "Break":
        return 1 + inner(value);
    case "Throw":
        return 2 + inner(value);
    case "If":
        return 3 + inner(value);
    case "While":
        return 4 + inner(value);
    case "DoWhile":
        return 5 + inner(value);
    case "For":
        return 6 + inner(value);
    default:
        return 7 + inner(value);
    }
}
noInline(outer);

for (var i = 0; i < 3e5; ++i) {
    shouldBe(outer("Do" + "While"), 10);
    shouldBe(outer("F" + "or"), 12);
    shouldBe(outer(""), 14);
    shouldBe(outer("TEST"), 14);
}
