import * as namespace from "./namespace-ambiguous/ambiguous.js"
import { shouldBe, shouldThrow } from "./resources/assert.js"

// Ambiguous name is omitted from the namespace.
shouldBe(namespace.Cocoa, undefined);
shouldBe('Cocoa' in namespace, false);
