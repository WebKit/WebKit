import { shouldBe } from "./resources/assert.js";

globalThis.moduleUsedByAAndBInvocations = 0;

const promises = [];
const promiseThatUsedToFail = import("./top-level-await-regress-242740/module-b.js");

promises.push(promiseThatUsedToFail);
promises.push(import("./top-level-await-regress-242740/module-used-by-a-and-b.js").then(({ someConst, someFunction }) => {
  shouldBe(someConst, "someConst");
  shouldBe(someFunction(), false);
}));

await Promise.allSettled(promises);
await promiseThatUsedToFail;

shouldBe(globalThis.moduleUsedByAAndBInvocations, 1);
