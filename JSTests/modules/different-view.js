import { shouldBe, shouldThrow } from "./resources/assert.js"

shouldThrow(() => {
    loadModule('./different-view/main.js');
}, `SyntaxError: Importing binding name 'A' cannot be resolved due to ambiguous multiple bindings.`);


