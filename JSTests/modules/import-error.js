import { shouldBe, shouldThrow } from "./resources/assert.js"

shouldThrow(() => {
    loadModule('./import-error/import-not-found.js');
}, `SyntaxError: Importing binding name 'B' is not found.`);

shouldThrow(() => {
    loadModule('./import-error/import-ambiguous.js');
}, `SyntaxError: Importing binding name 'B' cannot be resolved due to ambiguous multiple bindings.`);

shouldThrow(() => {
    loadModule('./import-error/import-default-from-star.js');
}, `SyntaxError: Importing binding name 'default' cannot be resolved by star export entries.`);
