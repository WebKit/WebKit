import { Cocoa, Cappuccino, Matcha } from "./scopes/drink.js"
import { shouldBe } from "./resources/assert.js";

{
    let Cocoa = 42;
    shouldBe(Cocoa, 42);
}
shouldBe(Cocoa, 'Cocoa');
shouldBe(Cappuccino, 'Cappuccino');
shouldBe(Matcha, 'Matcha');

var global = Function("return this")();

(function () {
    var Cocoa = 42;
    let Cappuccino = 'CPO';
    shouldBe(Cocoa, 42);
    shouldBe(Cappuccino, 'CPO');
    shouldBe(Matcha, 'Matcha');
    {
        const Matcha = 50;
        shouldBe(Matcha, 50);
        shouldBe(Object, global.Object);
    }
    shouldBe(Object, global.Object);
}());
shouldBe(Object, global.Object);
