import { Cappuccino } from "./scopes/drink.js"
import { shouldBe } from "./resources/assert.js";

// Separate test from scopes.js since direct eval can taint variables.
var global = Function("return this")();
var globalEval = (0, eval);
global.Cappuccino = 'Global Scope';
shouldBe(Cappuccino, 'Cappuccino');

(function () {
    let Cappuccino = 'Function Scope';
    shouldBe(Cappuccino, 'Function Scope');
    {
        let Cappuccino = 'Block Scope';
        {
            (function () {
                shouldBe(Cappuccino, 'Block Scope');
                shouldBe(eval(`Cappuccino`), 'Block Scope');
            }());
        }
    }
    shouldBe(Object, global.Object);
}());
shouldBe(Object, global.Object)
