import { shouldBe, shouldThrow } from "../resources/assert.js";
import { addLexical, addHoisted } from "./1.js";

// 1.js has not been evaluated yet, but calling its functions links their CodeBlocks.
shouldThrow(() => {
    addLexical(0);
}, `ReferenceError: Cannot access 'lexical' before initialization.`);
shouldBe(addHoisted(0), undefined);
