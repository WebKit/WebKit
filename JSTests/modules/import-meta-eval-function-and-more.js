import { shouldThrow } from "./resources/assert.js";

shouldThrow(() => {
    eval('import.meta');
}, `SyntaxError: import.meta is only valid inside modules.`);

shouldThrow(() => {
    new Function('import.meta')();
}, `SyntaxError: import.meta is only valid inside modules.`);

shouldThrow(() => {
    (void 0, eval)('import.meta');
}, `SyntaxError: import.meta is only valid inside modules.`);
