import { shouldThrow } from "./resources/assert.js"

shouldThrow(() => {
    loadModule('./namespace-error/namespace-local-error-should-hide-global-ambiguity.js');
}, `SyntaxError: Indirectly exported binding name 'default' cannot be resolved by star export entries.`);
