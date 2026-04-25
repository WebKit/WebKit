import { aValue } from "./a.js";
import { shouldThrow } from "../resources/assert.js"

export const bValue = 100;

shouldThrow(() => {
    eval("aValue");
}, `ReferenceError: Cannot access 'aValue' before initialization.`);
