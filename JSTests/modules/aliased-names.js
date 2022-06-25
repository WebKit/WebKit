import { a, b, change } from './aliased-names/main.js'
import { shouldBe, shouldThrow } from "./resources/assert.js";

shouldBe(a, 42);
shouldBe(b, 42);
change(400);
shouldBe(a, 400);
shouldBe(b, 400);
