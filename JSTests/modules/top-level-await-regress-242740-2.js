import { shouldBe } from "./resources/assert.js";

globalThis.moduleUsedByAAndBInvocations = 0;

const promiseThatUsedToFail = import("./top-level-await-regress-242740/module-b.js");
const { someConst, someFunction } = await import("./top-level-await-regress-242740/module-used-by-a-and-b.js");
shouldBe(someConst, "someConst");
shouldBe(someFunction(), false);

await promiseThatUsedToFail;

shouldBe(globalThis.moduleUsedByAAndBInvocations, 1);
