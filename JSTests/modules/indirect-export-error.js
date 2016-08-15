import { shouldBe, shouldThrow } from "./resources/assert.js"

shouldThrow(() => {
    loadModule('./indirect-export-error/indirect-export-not-found.js');
}, `SyntaxError: Indirectly exported binding name 'B' is not found.`);

shouldThrow(() => {
    loadModule('./indirect-export-error/indirect-export-ambiguous.js');
}, `SyntaxError: Indirectly exported binding name 'B' cannot be resolved due to ambiguous multiple bindings.`);

shouldThrow(() => {
    loadModule('./indirect-export-error/indirect-export-default.js');
}, `SyntaxError: Indirectly exported binding name 'default' cannot be resolved by star export entries.`);
