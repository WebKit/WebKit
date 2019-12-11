import { shouldThrow, shouldBe } from "./resources/assert.js";

// Module code is always strict code.
shouldThrow(() => {
    eval("with(value) { }");
}, `SyntaxError: 'with' statements are not valid in strict mode.`);

// When calling the indirect eval / Function constructor, its scope is not the module scope.
var moduleVariable = 42;

shouldBe(eval("moduleVariable"), 42);

shouldThrow(() => {
    (0, eval)("moduleVariable");
}, `ReferenceError: Can't find variable: moduleVariable`);

shouldThrow(() => {
    (Function("moduleVariable"))();
}, `ReferenceError: Can't find variable: moduleVariable`);
