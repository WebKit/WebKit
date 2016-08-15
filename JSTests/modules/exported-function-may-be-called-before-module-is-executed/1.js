import { add, raise } from "./2.js"
import { shouldBe, shouldThrow } from "../resources/assert.js";

shouldBe(typeof add, 'function');
shouldBe(typeof raise, 'function');

shouldBe(add(10, 32), 42);

shouldThrow(() => {
    // add, and raise functions are exported and can be used.
    // But module "2"'s body is not executed yet!!!!
    // raise function touches the lexical variable in the module "2", so TDZ
    // error should be raised.
    raise();
}, `ReferenceError: Cannot access uninitialized variable.`);
