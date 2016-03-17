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

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    function test() {
        return eval('super');
    }

    shouldThrow(() => test(), `SyntaxError: super is only valid inside functions.`);
}());

(function () {
    class B {
        gen() {
            return 42;
        }
    }

    class A extends B {
        *gen() {
            return eval('super.gen()');
        }
    }

    let a = new A();
    shouldBe(a.gen().next().value, 42);
}());

(function () {
    class B {
        gen() {
            return 42;
        }
    }

    class A extends B {
        *gen() {
            yield super.gen();
        }
    }

    let a = new A();
    shouldBe(a.gen().next().value, 42);
}());

(function () {
    class B {
        gen() {
            return 42;
        }
    }

    class A extends B {
        *gen() {
            yield super.gen();
        }
    }

    let a = new A();
    shouldThrow(() => {
        new a.gen();
    }, `TypeError: function is not a constructor (evaluating 'new a.gen()')`);
}());
