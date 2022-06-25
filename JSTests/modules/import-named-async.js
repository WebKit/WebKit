import { shouldBe } from "./resources/assert.js";
import { async } from "./import-named-async/target.js"

shouldBe(async, 42);
