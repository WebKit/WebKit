//@ requireOptions("--useImportDefer=1")
import { shouldBe, shouldThrow } from "./resources/assert.js";

// import.defer expects call arguments, mirroring import().
shouldThrow(() => eval("import.defer"), "SyntaxError: Unexpected end of script");
shouldThrow(() => eval("import.defer.foo"), "SyntaxError: Unexpected token '.'. import call expects one or two arguments.");
shouldThrow(() => eval("import.defer()"), "SyntaxError: Unexpected token ')'");
shouldThrow(() => eval("new import.defer('./import-defer/dep.js')"), "SyntaxError: Cannot use new with import.");
shouldThrow(() => eval("import.deferred('foo')"), "SyntaxError: Unexpected identifier 'deferred'. \"import.\" can only be followed with meta or defer.");

// Trailing commas and the second options argument are accepted, like import().
shouldBe(typeof import.defer("./import-defer/dep.js",), "object");
shouldBe(typeof import.defer("./import-defer/dep.js", { with: {} }), "object");
shouldBe(typeof import.defer("./import-defer/dep.js", { with: {} },), "object");

// Specifier ToString abrupt completion rejects (instead of throwing synchronously).
let rejected;
try {
    await import.defer({ toString() { throw new Error("specifier"); } });
} catch (error) {
    rejected = error;
}
shouldBe(rejected.message, "specifier");
