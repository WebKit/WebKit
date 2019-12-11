import * as namespace from "./A.js"
import { shouldBe, shouldThrow } from "../resources/assert.js";

export const B = 256;

shouldThrow(() => {
    print(namespace.A);
}, `ReferenceError: Cannot access uninitialized variable.`);

shouldThrow(() => {
    Reflect.getOwnPropertyDescriptor(namespace, 'A');
}, `ReferenceError: Cannot access uninitialized variable.`);

// Not throw any errors even if the field is not initialized yet.
shouldBe('A' in namespace, true);
shouldBe('hello' in namespace, false);

export function later() {
    shouldBe(namespace.A, 42);
}
