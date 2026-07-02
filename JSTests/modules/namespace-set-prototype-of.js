import * as namespace from "./namespace-set-prototype-of.js"
import { shouldBe, shouldThrow } from "./resources/assert.js";

shouldThrow(() => {
    Object.setPrototypeOf(namespace, {});
}, `TypeError: Cannot set prototype of immutable prototype object`);

shouldBe(Reflect.setPrototypeOf(namespace, {}), false);
shouldBe(Reflect.setPrototypeOf(namespace, null), true);
shouldBe(Object.setPrototypeOf(namespace, null), namespace);
