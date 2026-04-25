import {shouldBe} from "./resources/assert.js";

let array = [];
for (let i = 0; i < 4000000; i++) {
    array.push(i);
}

for (let i = 0; i < 4000000; i++) {
    shouldBe(array[i], i);
}
