import { shouldBe } from "./resources/assert.js";
import { Cocoa as async } from "./import-named-async-as.js"

export let Cocoa = 42;
shouldBe(async, 42);
shouldBe(Cocoa, 42);
