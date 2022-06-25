import { shouldBe } from "./resources/assert.js";
import * as async from "./import-namespace-async.js"

export let Cocoa = 42;
shouldBe(async.Cocoa, 42);
