import { shouldBe } from "./resources/assert.js"

import('./different-view/main.js').then($vm.abort, function (error) {
    shouldBe(String(error), `SyntaxError: Importing binding name 'A' cannot be resolved due to ambiguous multiple bindings.`);
}).catch($vm.abort);
