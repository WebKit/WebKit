function inner(value)
{
    switch (value) {
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
    }
}

function outer(value)
{
    return inner(value);
}
noInline(outer);

for (var i = 0; i < 1e6; ++i) {
    outer("DoWhile" + "Statement");
    outer("For" + "Statement");
    outer("");
}
