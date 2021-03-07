import { shouldBe, shouldThrow } from "./resources/assert.js";
import * as ns from "./arbitrary-module-names/export-indexed.js";

(() => {
    for (let i = 0; i < 1e5; i++) {
        shouldBe(ns[0], 0);
        shouldBe(Reflect.get(ns, 1), 1);
        shouldBe(ns[2], undefined);

        shouldThrow(() => { ns[0] = 1; }, `TypeError: Attempted to assign to readonly property.`);
        shouldBe(Reflect.set(ns, 1, 1), false);
        shouldThrow(() => { ns[2] = 2; }, `TypeError: Attempted to assign to readonly property.`);

        shouldBe(0 in ns, true);
        shouldBe(Reflect.has(ns, 1), true);
        shouldBe(2 in ns, false);

        shouldThrow(() => { delete ns[0]; }, `TypeError: Unable to delete property.`);
        shouldBe(Reflect.deleteProperty(ns, 1), false);
        shouldBe(delete ns[2], true);
    }
})();
