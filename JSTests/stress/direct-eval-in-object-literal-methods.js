function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let object = {
        n()
        {
            return 42;
        }
    };

    let derived = {
        m()
        {
            return eval("super.n()");
        }
    };
    Object.setPrototypeOf(derived, object);
    shouldBe(derived.m(), 42);
    // Cached.
    shouldBe(derived.m(), 42);
}

{
    let object = {
        l()
        {
            return 42;
        }
    };

    let derived = {
        m()
        {
            return eval("super.l()");
        }
    };
    Object.setPrototypeOf(derived, object);
    shouldBe(derived.m(), 42);
    // Cached.
    shouldBe(derived.m(), 42);

    class Parent {
        l()
        {
            return 55;
        }
    }

    class Derived extends Parent {
        m()
        {
            return eval("super.l()");
        }
    }
    let instance = new Derived();
    // Under the strict code, not cached.
    shouldBe(instance.l(), 55);
    shouldBe(instance.l(), 55);
}
