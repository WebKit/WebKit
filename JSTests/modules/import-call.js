import { shouldBe } from "./resources/assert.js"

import("./import-call/main.js").then((result) => {
    shouldBe(result.Cocoa, 42);
});
