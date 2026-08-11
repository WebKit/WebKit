import { shouldBe } from "./resources/assert.js"
import test from "./async-generator-default/module.js"

shouldBe(typeof test, "function");
let asyncGenerator = test();
shouldBe(asyncGenerator.__proto__, test.prototype);
shouldBe(asyncGenerator.next() instanceof Promise, true);
