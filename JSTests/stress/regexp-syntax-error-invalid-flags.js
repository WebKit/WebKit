function shouldThrowSyntaxError(script) {
    let error;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }

    if (!error)
        throw new Error('not thrown');
    if (String(error) !== 'SyntaxError: Invalid regular expression: invalid flags')
        throw new Error(`bad error: ${String(error)}`);
}

shouldThrowSyntaxError('/Hello/cocoa');
shouldThrowSyntaxError('/a/Z');
shouldThrowSyntaxError('/./ii');
