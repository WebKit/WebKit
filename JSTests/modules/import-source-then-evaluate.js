//@ requireOptions("--useSourcePhaseImports=1", "--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

let sourceError;
try {
    await import("./import-source/source-importer.js");
} catch (error) {
    sourceError = error;
}
if (!(sourceError instanceof SyntaxError))
    throw new Error("static import source of a JS module should be a SyntaxError, got " + sourceError);

const ns = await import("./import-source/has-dep.js");
shouldBe(ns.value, 42);

let deferSourceError;
try {
    await import("./import-source/source-importer-defer.js");
} catch (error) {
    deferSourceError = error;
}
if (!(deferSourceError instanceof SyntaxError))
    throw new Error("static import source of a JS module should be a SyntaxError, got " + deferSourceError);

const deferred = await import.defer("./import-source/has-dep-defer.js");
shouldBe(deferred.value, 7);
