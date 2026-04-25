function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const str8 = "hello" + " world";
const str16 = "こん" + "にちは";

let count1 = 0;
let count2 = 0;

for (let i = 0; i < 1e6; i++) {
    const useStr8 = i % 4 === 0;
    const useStr16 = i % 4 === 1;
    const s1 = useStr8 ? str8 : str16;
    const s2 = useStr16 ? str16 : str8;
    
    if (s1 === s2) {
        count1++;
    } else {
        count2++;
    }
}

shouldBe(count1, 1e6 / 2);
shouldBe(count2, 1e6 / 2);