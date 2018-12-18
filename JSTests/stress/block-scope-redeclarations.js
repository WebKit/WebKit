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

shouldThrowSyntaxError('{ var x; let x; }');
shouldThrowSyntaxError('{ { var x; } let x; }');
shouldThrowSyntaxError('{ { { var x; } } let x; }');
shouldThrowSyntaxError('{ let x; var x; }');
shouldThrowSyntaxError('{ let x; { var x; } }');
shouldThrowSyntaxError('{ let x; { { var x; } } }');

shouldNotThrow('{ var x; { let x; } }');
shouldNotThrow('{ var x; { { let x; } } }');
shouldNotThrow('{ { let x; } var x; }');
shouldNotThrow('{ { { let x; } } var x; }');

shouldThrowSyntaxError('{ var x; const x = 0; }');
shouldThrowSyntaxError('{ { var x; } const x = 0; }');
shouldThrowSyntaxError('{ { { var x; } } const x = 0; }');
shouldThrowSyntaxError('{ const x = 0; var x; }');
shouldThrowSyntaxError('{ const x = 0; { var x; } }');
shouldThrowSyntaxError('{ const x = 0; { { var x; } } }');

shouldNotThrow('{ var x; { const x = 0; } }');
shouldNotThrow('{ var x; { { const x = 0; } } }');
shouldNotThrow('{ { const x = 0; } var x; }');
shouldNotThrow('{ { { const x = 0; } } var x; }');
