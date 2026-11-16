//@ requireOptions("--useExplicitResourceManagement=true")

function shouldThrowSyntaxError(code) {
    var threw = false;
    try {
        eval(code);
    } catch (e) {
        if (!(e instanceof SyntaxError))
            throw new Error(`wrong error type: ${e.constructor.name}: ${e.message}`);
        threw = true;
    }
    if (!threw)
        throw new Error(`did not throw: ${code}`);
}

function shouldNotThrowSyntaxError(code) {
    try {
        eval(code);
    } catch (e) {
        if (e instanceof SyntaxError)
            throw new Error(`unexpected SyntaxError: ${e.message}: ${code}`);
    }
}

shouldThrowSyntaxError(`await using x = null;`);
shouldThrowSyntaxError(`function f() { await using x = null; }`);
shouldThrowSyntaxError(`function* f() { await using x = null; }`);
shouldThrowSyntaxError(`() => { await using x = null; }`);

// `await [no LT] using` constraint: with a line break, `await` becomes an AwaitExpression
// whose operand is `using` (an identifier expression), and the rest is a separate statement.
// This is not a SyntaxError — the following should parse but run as distinct statements.
{
    let calledDispose = false;
    let using = { [Symbol.dispose]() { calledDispose = true; } };
    (async function() {
        let x;
        await
        using
        x = null;
        if (x !== null) throw new Error("line break: x should be null");
    })();
    drainMicrotasks();
    if (calledDispose) throw new Error("line break: should not have called dispose");
}

shouldThrowSyntaxError(`async function f() { await using [a] = null; }`);
shouldThrowSyntaxError(`async function f() { await using {a} = null; }`);

shouldThrowSyntaxError(`async function f() { await using x; }`);

shouldThrowSyntaxError(`async function f() { for (await using x in obj) {} }`);
shouldThrowSyntaxError(`async function f() { for (await using x; ; ) {} }`);

shouldThrowSyntaxError(`async function f() { switch (1) { case 1: await using x = null; } }`);

shouldNotThrowSyntaxError(`async function f() { await using x = null; }`);
shouldNotThrowSyntaxError(`async function* f() { await using x = null; }`);
shouldNotThrowSyntaxError(`async function f() { { await using x = null; } }`);
shouldNotThrowSyntaxError(`async function f() { for (await using x of []) {} }`);
shouldNotThrowSyntaxError(`async function f() { for await (await using x of []) {} }`);
shouldNotThrowSyntaxError(`async function f() { switch (1) { case 1: { await using x = null; } } }`);
shouldNotThrowSyntaxError(`async function f() { await using a = null, b = null; }`);
