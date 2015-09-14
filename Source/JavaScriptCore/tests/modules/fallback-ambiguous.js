//        +-----------------+
//        |                 |
//        v                 |
//       (A) -> (C) -> (D) *+-> [E]
//        *      ^
//        |      |
//        v      @
//       (B)
import { shouldThrow } from "./resources/assert.js"
shouldThrow(() => {
    loadModule("./fallback-ambiguous/main.js");
}, `SyntaxError: Indirectly exported binding name 'A' cannot be resolved due to ambiguous multiple bindings.`);
