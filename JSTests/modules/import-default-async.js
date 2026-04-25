import { shouldBe } from "./resources/assert.js";
import async from "./import-default-async.js"

export default 42;

shouldBe(async, 42);
