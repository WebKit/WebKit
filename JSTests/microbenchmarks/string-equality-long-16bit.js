function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const longStr16 = "あ".repeat(100) + "い".repeat(100) + "う".repeat(100);

let count1 = 0;
let count2 = 0;

for (let i = 0; i < 1e4; i++) {
    const s = i % 2 === 0 
        ? "あ".repeat(100) + "い".repeat(100) + "え".repeat(100)
        : "あ".repeat(100) + "い".repeat(100) + "う".repeat(100);
    
    if (longStr16 === s) {
        count1++;
    } else {
        count2++;
    }
}

shouldBe(count1, 1e4 / 2);
shouldBe(count2, 1e4 / 2);