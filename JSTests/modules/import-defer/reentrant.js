import defer * as self from "./reentrant.js";
let caught;
try {
    self.value;
} catch (e) {
    caught = e;
}
export const result = caught;
export const value = 0;
