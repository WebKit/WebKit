//        +-----------------+
//        |                 |
//        v                 |
//       (A) -> (C) -> (D) *+-> [E]
//        *      ^
//        |      |
//        v      @
//       (B)
import { shouldBe } from "./resources/assert.js"

import('./fallback-ambiguous/main.js').then($vm.abort, function (error) {
    shouldBe(String(error), `SyntaxError: Indirectly exported binding name 'A' cannot be resolved due to ambiguous multiple bindings.`);
}).catch($vm.abort);
