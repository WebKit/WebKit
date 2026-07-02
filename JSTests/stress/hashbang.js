#!foo

function shouldNotThrow(script) {
    eval(script);
}

function shouldThrowSyntaxError(script) {
    let error;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }

    if (!(error instanceof SyntaxError))
        throw new Error('Expected SyntaxError!');
}

shouldNotThrow('#!foo');
shouldThrowSyntaxError(' #!foo');
shouldThrowSyntaxError('\n#!foo');
shouldThrowSyntaxError('Function("#!foo")');
