import { variable, constVariable, letVariable, functionDeclaration, classDeclaration } from "./imported-bindings-are-immutable/bindings.js"
import { shouldBe, shouldThrow } from "./resources/assert.js"

shouldBe(variable, 'Cocoa');
shouldThrow(() => {
    variable = 42;
}, `TypeError: Attempted to assign to readonly property.`);

shouldBe(constVariable, 'Cocoa');
shouldThrow(() => {
    constVariable = 42;
}, `TypeError: Attempted to assign to readonly property.`);

shouldBe(letVariable, 'Cocoa');
shouldThrow(() => {
    letVariable = 42;
}, `TypeError: Attempted to assign to readonly property.`);

shouldBe(typeof functionDeclaration, 'function');
shouldThrow(() => {
    functionDeclaration = 42;
}, `TypeError: Attempted to assign to readonly property.`);

shouldBe(typeof classDeclaration, 'function');
shouldThrow(() => {
    classDeclaration = 42;
}, `TypeError: Attempted to assign to readonly property.`);


function reference(read) {
    if (read)
        return letVariable;
    else
        letVariable = "Cocoa";
}
noInline(reference);

for (var i = 0; i < 10000; ++i)
    reference(true);

shouldThrow(() => {
    reference(false);
}, `TypeError: Attempted to assign to readonly property.`);
