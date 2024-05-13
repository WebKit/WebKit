import { shouldBe } from "../resources/assert.js";

const { someConst, someFunction } = await import("./module-used-by-a-and-b.js");
shouldBe(someConst, "someConst");
shouldBe(someFunction(), false);
