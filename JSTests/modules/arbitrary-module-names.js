import {
    "delete" as deleting,
    "if" as ok1,
    "ok2" as ok2
} from "./arbitrary-module-names/export.js";
import { "var" as variable } from "./arbitrary-module-names/export.js";
import { shouldBe, shouldThrow } from "./resources/assert.js";

shouldBe(deleting, 42);
shouldBe(variable.test, 40);
shouldBe(ok1, 400);
shouldBe(ok2, 401);
