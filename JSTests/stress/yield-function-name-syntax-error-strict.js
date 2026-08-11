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

shouldThrowSyntaxError('function yield() { "use strict"; }');
shouldThrowSyntaxError('function* yield() { "use strict"; }');
shouldThrowSyntaxError('async function yield() { "use strict"; }');
