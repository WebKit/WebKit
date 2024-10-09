function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}.`);
}

let eval;

(function thisValue() {
    eval = function() { return this; };
    shouldBe(eval(), globalThis);

    eval = function() { "use strict"; return this; };
    shouldBe(eval(), undefined);

    const bindingObject = { eval };
    with (bindingObject) {
        shouldBe(eval(), bindingObject);
    }
})();
