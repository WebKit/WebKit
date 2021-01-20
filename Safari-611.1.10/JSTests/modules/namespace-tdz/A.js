import * as namespace from "./B.js"
import { shouldBe, shouldThrow } from "../resources/assert.js";

export const A = 42;

shouldBe(JSON.stringify(Reflect.getOwnPropertyDescriptor(namespace, 'B')), `{"value":256,"writable":true,"enumerable":true,"configurable":false}`);

shouldBe(namespace.B, 256);
namespace.later();

