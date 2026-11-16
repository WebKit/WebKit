//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

// SCC {a, b}: a has TLA and rejects; b is the non-root member.
// After the cycle root (a) fails asynchronously, accessing the deferred namespace of b
// must rethrow a's evaluation error rather than silently succeeding, because Evaluate()
// redirects to [[CycleRoot]].

let importError;
await import("./import-defer/cycle-a.js").catch(e => importError = e);
shouldBe(typeof importError, "object");
shouldBe(importError.someError, "tla-reject");

const { nsB } = await import("./import-defer/cycle-b-exporter.js");

let accessError;
try {
    nsB.value;
} catch (e) {
    accessError = e;
}
shouldBe(accessError, importError);
