description(
"This test checks behavior with expressions that create deep parse trees."
);

// At the moment, this tests only repeated "+" operator expression trees.
// There are lots of other cases wer could cover to make this a more thorough test.

// Use a variable to decrease the chance the test will be invalid due to constnat folding.
var letterA = 'a';

function repeatedExpression(value, operator, count)
{
    var expression = value;
    for (var i = 1; i < count; ++i)
        expression += ' ' + operator + ' ' + value;
    return expression;
}

function repeatedString(value, count)
{
    var result = "";
    for (var i = 0; i < count; ++i)
        result += value;
    return result;
}

shouldBe('eval(repeatedExpression("letterA", "+", 100))', 'repeatedString("a", 100)');
shouldBe('eval(repeatedExpression("letterA", "+", 1000))', 'repeatedString("a", 1000)');
shouldThrow('eval(repeatedExpression("letterA", "+", 10000))', '"SyntaxError: Expression too deep"');
shouldThrow('eval(repeatedExpression("letterA", "+", 100000))', '"SyntaxError: Expression too deep"');

var successfullyParsed = true;
