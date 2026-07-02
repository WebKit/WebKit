import { aValue } from "./sync-from-async/a.js";

// Normally, this should cause a ReferenceError (which we catch and assert for).
// In a debug build of JSC, this used to cause an assertion failure in the module loader.
// This test will test that the assertion failure doesn't occur.
