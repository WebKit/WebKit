function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const str16 = "こん" + "にちは";

let count1 = 0;
let count2 = 0;

for (let i = 0; i < 1e6; i++) {
    const s = i % 2 === 0 ? "こん" + "ばんは" : "こん" + "にちは";
    if (str16 === s) {
        count1++;
    } else {
        count2++;
    }
}

shouldBe(count1, 1e6 / 2);
shouldBe(count2, 1e6 / 2);
