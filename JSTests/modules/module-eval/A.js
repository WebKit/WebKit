import { B } from "./B.js"
import { shouldThrow } from "../resources/assert.js"

export let A = "A";

shouldThrow(() => {
    eval("B");
}, `ReferenceError: Cannot access 'B' before initialization.`);
