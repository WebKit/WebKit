function test(s) {
    for (let i = 0; i < 100; i++) {
        let threw = false;
        try {
            let evalString = `try { throw new Error } catch(${s}) { }`;
            eval(evalString);
        } catch(e) {
            threw = e instanceof ReferenceError;
        }
        if (!threw)
            throw new Error("Bad test!");
    }
}

test("{a = a}");
test("{a = eval('a')}");
test("{a = eval('a + a')}");
test("{a = eval('b'), b}");
test("{a = eval('b + b'), b}");
test("{a = eval('b + b'), b = 20}");
test("{a = b+b, b = 20}");
