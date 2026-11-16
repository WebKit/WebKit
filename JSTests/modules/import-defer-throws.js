//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

import defer * as ns from "./import-defer/throws.js";

function read(obj) {
    try {
        return obj.value;
    } catch (e) {
        return e;
    }
}
noInline(read);

// Every access must re-throw the cached [[EvaluationError]]; the IC must not cache a
// successful binding load that bypasses the throw.
let firstError = read(ns);
shouldBe(typeof firstError, "object");
shouldBe(firstError.someError, "deferred-throw");
for (let i = 0; i < 1e4; ++i)
    shouldBe(read(ns), firstError);
