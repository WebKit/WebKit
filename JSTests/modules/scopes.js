import { Cocoa, Cappuccino, Matcha } from "./scopes/drink.js"
import { shouldBe } from "./resources/assert.js";

var global = Function("return this")();
var globalEval = (0, eval);
global.Cappuccino = 'Global Scope';

{
    let Cocoa = 42;
    shouldBe(Cocoa, 42);
}
shouldBe(Cocoa, 'Cocoa');
shouldBe(Cappuccino, 'Cappuccino'); // Module Scope.
shouldBe(Matcha, 'Matcha');

(function () {
    var Cocoa = 42;
    let Cappuccino = 'Function Scope';
    shouldBe(Cocoa, 42);
    shouldBe(Cappuccino, 'Function Scope');
    shouldBe(Matcha, 'Matcha');
    {
        let Cappuccino = 'Block Scope';
        const Matcha = 50;
        shouldBe(Matcha, 50);
        shouldBe(Object, global.Object);
        {
            (function () {
                shouldBe(Cappuccino, 'Block Scope');
                shouldBe(globalEval(`Cappuccino`), 'Global Scope');
                shouldBe(Function(`return Cappuccino`)(), 'Global Scope');
            }());
        }
    }
    shouldBe(Object, global.Object);
}());
shouldBe(Object, global.Object);
