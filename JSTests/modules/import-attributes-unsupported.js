//@ requireOptions("--useImportAttributes=1")
import { shouldBe } from "./resources/assert.js";

{
    let error = null;
    try {
        await import("./resources/import-attributes-unsupported-1.js");
    } catch (e) {
        error = e;
    }
    shouldBe(String(error), `SyntaxError: Unexpected keyword 'true'. Expected an attribute value.`);
}
{
    let error = null;
    try {
        await import("./resources/import-attributes-unsupported-2.js");
    } catch (e) {
        error = e;
    }
    shouldBe(String(error), `SyntaxError: Import attribute "unsupported" is not supported`);
}
{
    let error = null;
    try {
        await import("./resources/import-attributes-unsupported-3.js");
    } catch (e) {
        error = e;
    }
    shouldBe(String(error), `SyntaxError: Unexpected number '42'. Expected an attribute key.`);
}
