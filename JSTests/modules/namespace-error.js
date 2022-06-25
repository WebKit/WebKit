import { shouldBe } from "./resources/assert.js"

import('./namespace-error/namespace-local-error-should-hide-global-ambiguity.js').then($vm.abort, function (error) {
    shouldBe(String(error), `SyntaxError: Indirectly exported binding name 'default' cannot be resolved by star export entries.`);
}).catch($vm.abort);
