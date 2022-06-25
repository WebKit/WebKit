import { shouldBe, shouldThrow, shouldNotThrow } from "./resources/assert.js"
import * as ns from "./module-namespace-object-define-own-property/module.js"

shouldThrow(() => {
    Object.defineProperty(ns, Symbol.unscopables, {
        value: 42
    });
}, `TypeError: Attempting to define property on object that is not extensible.`);

shouldThrow(() => {
    Object.defineProperty(ns, Symbol.toStringTag, {
        value: 42
    });
}, `TypeError: Attempting to change value of a readonly property.`);

shouldThrow(() => {
    Object.defineProperty(ns, "variable", {
        get: function () { }
    });
}, `TypeError: Cannot change module namespace object's binding to accessor`);

shouldThrow(() => {
    Object.defineProperty(ns, "variable", {
        writable: false
    });
}, `TypeError: Cannot change module namespace object's binding to non-writable attribute`);

shouldThrow(() => {
    Object.defineProperty(ns, "variable", {
        enumerable: false
    });
}, `TypeError: Cannot replace module namespace object's binding with non-enumerable attribute`);

shouldThrow(() => {
    Object.defineProperty(ns, "variable", {
        configurable: true
    });
}, `TypeError: Cannot replace module namespace object's binding with configurable attribute`);

shouldThrow(() => {
    Object.defineProperty(ns, "variable", {
        value: 43
    });
}, `TypeError: Cannot replace module namespace object's binding's value`);

shouldNotThrow(() => {
    Reflect.defineProperty(ns, "variable", { value: 42 });
});

shouldBe(Reflect.defineProperty(ns, "variable", { value: 42 }), true);
