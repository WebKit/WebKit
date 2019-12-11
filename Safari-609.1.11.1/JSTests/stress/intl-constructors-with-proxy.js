//@ skip if $hostOS == "windows"

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    function Empty() {
    }
    let proxy = new Proxy(Empty, {});

    shouldBe(Reflect.construct(Intl.Collator, [], proxy) instanceof Empty, true);
    shouldBe(Reflect.construct(Intl.Collator, [], proxy).__proto__, Empty.prototype);

    shouldBe(Reflect.construct(Intl.NumberFormat, [], proxy) instanceof Empty, true);
    shouldBe(Reflect.construct(Intl.NumberFormat, [], proxy).__proto__, Empty.prototype);

    shouldBe(Reflect.construct(Intl.DateTimeFormat, [], proxy) instanceof Empty, true);
    shouldBe(Reflect.construct(Intl.DateTimeFormat, [], proxy).__proto__, Empty.prototype);
}

{
    function Empty() {
    }
    Empty.prototype = null;

    let proxy = new Proxy(Empty, {});

    shouldBe(Reflect.construct(Intl.Collator, [], proxy) instanceof Intl.Collator, true);
    shouldBe(Reflect.construct(Intl.Collator, [], proxy).__proto__, Intl.Collator.prototype);

    shouldBe(Reflect.construct(Intl.NumberFormat, [], proxy) instanceof Intl.NumberFormat, true);
    shouldBe(Reflect.construct(Intl.NumberFormat, [], proxy).__proto__, Intl.NumberFormat.prototype);

    shouldBe(Reflect.construct(Intl.DateTimeFormat, [], proxy) instanceof Intl.DateTimeFormat, true);
    shouldBe(Reflect.construct(Intl.DateTimeFormat, [], proxy).__proto__, Intl.DateTimeFormat.prototype);
}
