function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class Hello extends Object {
    constructor()
    {
        super();
    }
}

var hello = new Hello();
shouldBe(hello.__proto__, Hello.prototype);

shouldBe(Reflect.construct(Object, [], Hello).__proto__, Hello.prototype);

gc(); // Regression test for https:/webkit.org/b/160666.
