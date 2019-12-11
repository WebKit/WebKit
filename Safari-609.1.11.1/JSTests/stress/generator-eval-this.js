function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
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

function *gen() {
    yield eval("this");
}

shouldThrow(() => {
    var g = new gen();
    g.next().value;
}, `TypeError: function is not a constructor (evaluating 'new gen()')`);

class B { }

(function() {
    eval('this');
    eval('this');
}());

class A extends B {
    constructor()
    {
        return eval('this');
    }
}

shouldThrow(() => {
    new A();
}, `ReferenceError: Cannot access uninitialized variable.`);

class C {
    *generator()
    {
        yield eval('this');
    }
}

shouldThrow(() => {
    let c = new C();
    let g = new c.generator();
    g.next();
}, `TypeError: function is not a constructor (evaluating 'new c.generator()')`);

(function () {
    let c = new C();
    let g = c.generator();
    shouldBe(g.next().value, c);
}());
