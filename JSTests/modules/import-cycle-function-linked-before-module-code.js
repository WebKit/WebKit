import { shouldBe } from "./resources/assert.js";
import * as namespace from "./import-cycle-function-linked-before-module-code/1.js";
import { lexical, hoisted, addLexical, readLexical, addLexicalThreeTimes, addHoisted, readHoisted } from "./import-cycle-function-linked-before-module-code/1.js";

for (let i = 0; i < testLoopCount; ++i) {
    let base = i * 4;

    shouldBe(addLexical(1), base + 1);
    shouldBe(readLexical(), base + 1);
    shouldBe(lexical, base + 1);
    shouldBe(namespace.lexical, base + 1);

    shouldBe(addLexicalThreeTimes(), base + 4);
    shouldBe(readLexical(), base + 4);
    shouldBe(lexical, base + 4);
    shouldBe(namespace.lexical, base + 4);

    shouldBe(addHoisted(2), i * 2 + 2);
    shouldBe(readHoisted(), i * 2 + 2);
    shouldBe(hoisted, i * 2 + 2);
    shouldBe(namespace.hoisted, i * 2 + 2);
}
