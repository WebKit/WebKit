import { shouldBe } from "./resources/assert.js";

// Repeated dynamic import() of the same specifier from the same referrer
// must return the same Module Namespace Object (HostLoadImportedModule
// idempotency, https://tc39.es/ecma262/#sec-HostLoadImportedModule).
// Exercises the referrer.[[LoadedModules]] short-circuit in
// hostLoadImportedModule for both Realm and module referrers.

const first = await import("./dynamic-import-loaded-modules-cache/leaf.js");
shouldBe(first.value, 42);

for (let i = 0; i < 100; i++) {
    const ns = await import("./dynamic-import-loaded-modules-cache/leaf.js");
    shouldBe(ns, first);
    shouldBe(ns.value, 42);
}

// Same module via a fresh module-referrer (re-importer.js).
const re = await import("./dynamic-import-loaded-modules-cache/re-importer.js");
shouldBe(re.ns, first);

// Re-importing re-importer hits the cache as well.
const re2 = await import("./dynamic-import-loaded-modules-cache/re-importer.js");
shouldBe(re2, re);
