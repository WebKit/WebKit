function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    function basic() {
        return new.target;
    }

    shouldBe(basic(), undefined);
    shouldBe(new basic(), basic);

    function arrow() {
        return () => new.target;
    }

    shouldBe(arrow()(), undefined);
    shouldBe(new arrow()(), arrow);

    class Base {
        constructor()
        {
            this.value = new.target;
        }
    }

    class Derived extends Base {
        constructor()
        {
            super();
        }
    }

    var derived = new Derived();
    shouldBe(derived.value, Derived);
}());
(function () {
    function evaluate() {
        return eval(`new.target`);
    }
    shouldBe(evaluate(), undefined);
    shouldBe(new evaluate(), evaluate);
}());
