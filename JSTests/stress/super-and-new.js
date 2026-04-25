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

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test() {
    let async = {
        super: Object
    };
    class A extends Object {
        constructor()
        {
            super();
        }
        get hey()
        {
            return Object;
        }
        get hey2()
        {
            return Object;
        }
        super()
        {
            return Object;
        }
    }

    class B extends A {
        constructor()
        {
            super();
            shouldBe(typeof (new super.hey), "object");
            shouldBe(typeof (new super.hey()), "object");
            shouldBe(typeof (new super["hey2"]()), "object");
            shouldBe(typeof new super.super``, "object");
            shouldBe(typeof new async.super(), "object");
            shouldBe(typeof new.target.super(), "object");
        }

        static get super() { return Object; }
    }

    new B();
}

test();
testSyntaxError(`
class A extends Object {
    constructor()
    {
        new super();
    }
}
`, `SyntaxError: Cannot use new with super call.`);
testSyntaxError(`
class A extends Object {
    constructor()
    {
        new super;
    }
}
`, `SyntaxError: Cannot use new with super call.`);
testSyntaxError(`
class A extends Object {
    constructor()
    {
        new super?.ok();
    }
}
`, `SyntaxError: Cannot call constructor in an optional chain.`);
