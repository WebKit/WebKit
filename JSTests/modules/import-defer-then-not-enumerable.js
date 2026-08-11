//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

import defer * as ns from "./import-defer/dep.js";

// On a deferred namespace, [[GetOwnProperty]]("then") returns undefined (IsSymbolLikeNamespaceKey),
// so enumerable-only listings must exclude "then" while [[OwnPropertyKeys]] still includes it.
shouldBe(JSON.stringify(Object.keys(ns)), `["value"]`);
shouldBe(JSON.stringify(Object.getOwnPropertyNames(ns)), `["then","value"]`);
shouldBe(JSON.stringify(Reflect.ownKeys(ns).filter((key) => typeof key === "string")), `["then","value"]`);
shouldBe(Object.getOwnPropertyDescriptor(ns, "then"), undefined);
shouldBe(JSON.stringify(Object.entries(ns)), `[["value",1]]`);
shouldBe(JSON.stringify(Object.values(ns)), `[1]`);
shouldBe(JSON.stringify({ ...ns }), `{"value":1}`);

const forIn = [];
for (const key in ns)
    forIn.push(key);
shouldBe(JSON.stringify(forIn), `["value"]`);

// Evaluation has been triggered above; the namespace stays deferred, so "then" remains hidden.
shouldBe(ns.then, undefined);
shouldBe(JSON.stringify(Object.keys(ns)), `["value"]`);
