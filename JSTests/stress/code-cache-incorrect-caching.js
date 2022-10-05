function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}
var globalEval = eval;
var global = this;

// EvalContextType.
{
    function hello()
    {
        return eval('new.target');
    }
    shouldBe(hello(), undefined);
    shouldBe(hello(), undefined);
    shouldBe(hello(), undefined);
    globalEval(`
        var thrown = false;
        try {
            eval('new.target');
        } catch (e) {
            thrown = true;
            shouldBe(String(e), "SyntaxError: new.target is only valid inside functions or static blocks.");
        }
        shouldBe(thrown, true);
    `);

    var thrown = false;
    try {
        globalEval('new.target');
    } catch (e) {
        thrown = true;
        shouldBe(String(e), "SyntaxError: new.target is only valid inside functions or static blocks.");
    }
    shouldBe(thrown, true);
}

// DerivedContextType.
{
    var object = {
        hello()
        {
            return eval('super.ok');
        }
    };
    object.__proto__ = { ok: 42 };
    shouldBe(object.hello(), 42);

    var test = {
        hello: function () {
            return eval('super.ok');
        }
    };
    test.__proto__ = { ok: 42 };
    shouldThrow(function () {
        test.hello();
    }, `SyntaxError: super is not valid in this context.`);
}

// isArrowFunctionContext.
{
globalEval(`
    function ok()
    {
        return eval('this');
    }
    shouldBe(ok(), global)
    var hello = {
        hello()
        {
            var arrow = () => eval('this');
            shouldBe(arrow(), hello);
        }
    };
    hello.hello();
`);
}
