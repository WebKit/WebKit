function testSyntax(script) {
    try {
        eval(script);
    } catch (error) {
        if (error instanceof SyntaxError)
            throw new Error("Bad error: " + String(error));
    }
}

function testSyntaxError(script, message) {
    var error = null;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("Expected syntax error not thrown");

    if (String(error) !== message)
        throw new Error("Bad error: " + String(error));
}

{
    let tokens = [
        '-',
        '+',
        '~',
        '!',
        'typeof',
        'void',
        'delete',
    ];

    for (let token of tokens) {
        testSyntaxError(`
        function pow(a, b)
        {
            return ${token} a ** b;
        }
        `, `SyntaxError: Unexpected token '**'. Ambiguous unary expression in the left hand side of the exponentiation expression; parentheses must be used to disambiguate the expression.`);
    }
}

{
    let tokens = [
        '-',
        '+',
        '~',
        '!',
        'typeof',
        'void',
        'delete',
    ];

    for (let token of tokens) {
        testSyntax(`
        function pow(a, b)
        {
            return (${token} a) ** b;
        }
        `);
    }
}

{
    let tokens = [
        '++',
        '--',
    ];

    for (let token of tokens) {
        testSyntax(`
        function pow(a, b)
        {
            return ${token} a ** b;
        }
        `);
    }
}

{
    let tokens = [
        '++',
        '--',
    ];

    for (let token of tokens) {
        testSyntax(`
        function pow(a, b)
        {
            return a ${token} ** b;
        }
        `);
    }
}
