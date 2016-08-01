import * as namespace from "./default-value-case-should-be-copied/module.js"
import value from "./default-value-case-should-be-copied/module.js"
import { shouldBe } from "./resources/assert.js";

shouldBe(value, 42);
shouldBe(namespace.default, 42);

namespace.changeValue(5000);

shouldBe(value, 42);
shouldBe(namespace.default, 42);
