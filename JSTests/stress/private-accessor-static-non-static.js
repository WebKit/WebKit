function assert(a, e, m) {
    if (a !== e)
        throw new Error(m);
}

function assertSyntaxError(code) {
    try {
        eval(code);
        throw new Error("Code executed without throwing SyntaxError");
    } catch (e) {
        assert(e instanceof SyntaxError, true, e.message);
    }
}

assertSyntaxError(`
    class C {
        static get #m() {}
        set #m(v) {}
    }
`);

assertSyntaxError(`
    class C {
        get #m() {}
        static set #m(v) {}
    }
`);

assertSyntaxError(`
    class C {
        static set #m(v) {}
        get #m() {}
    }
`);

assertSyntaxError(`
    class C {
        set #m(v) {}
        static get #m() {}
    }
`);

