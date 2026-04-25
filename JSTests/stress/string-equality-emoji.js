function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const testLoopCount = 10000;

for (let i = 0; i < testLoopCount; i++) {
    const emoji1 = "🌸" + "🗾🍣";
    const emoji2 = "🌸" + "🗾🍣";
    const emoji3 = "🌸" + "🗾🍤";
    shouldBe(emoji1 === emoji2, true);
    shouldBe(emoji1 === emoji3, false);
    
    const face1 = "😀" + "😃😄";
    const face2 = "😀" + "😃😄";
    const face3 = "😀" + "😃😅";
    shouldBe(face1 === face2, true);
    shouldBe(face1 === face3, false);
    
    const mixed1 = "Hel" + "lo🌍World";
    const mixed2 = "Hel" + "lo🌍World";
    const mixed3 = "Hel" + "lo🌎World";
    shouldBe(mixed1 === mixed2, true);
    shouldBe(mixed1 === mixed3, false);
    
    const numEmoji1 = "1️⃣" + "2️⃣3️⃣";
    const numEmoji2 = "1️⃣" + "2️⃣3️⃣";
    const numEmoji3 = "1️⃣" + "2️⃣4️⃣";
    shouldBe(numEmoji1 === numEmoji2, true);
    shouldBe(numEmoji1 === numEmoji3, false);
    
    const flag1 = "🇯🇵" + "🇺🇸";
    const flag2 = "🇯🇵" + "🇺🇸";
    const flag3 = "🇯🇵" + "🇬🇧";
    shouldBe(flag1 === flag2, true);
    shouldBe(flag1 === flag3, false);
    
    const heart1 = "❤️" + "💙💚";
    const heart2 = "❤️" + "💙💚";
    const heart3 = "❤️" + "💙💛";
    shouldBe(heart1 === heart2, true);
    shouldBe(heart1 === heart3, false);
    
    const complex1 = "👨‍👩" + "‍👧‍👦";
    const complex2 = "👨‍👩" + "‍👧‍👦";
    shouldBe(complex1 === complex2, true);
    
    const emojiBase = "🎉";
    const dynamic1 = emojiBase + i.toString();
    const dynamic2 = emojiBase + i.toString();
    shouldBe(dynamic1 === dynamic2, true);
}