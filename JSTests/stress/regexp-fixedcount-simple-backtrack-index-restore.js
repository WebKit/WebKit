function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error("bad value: " + JSON.stringify(actual) + " expected: " + JSON.stringify(expected));
}

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe("xza".match(/(?:xz){2}|./), ["x"]);
    shouldBe("xz".match(/(?:xz){2}|./), ["x"]);
    shouldBe("xyzxyz".match(/(?:xyz){3}|(?:xyz){2}/), ["xyzxyz"]);
    shouldBe("abab".match(/(?:ab){3}|./g), ["a", "b", "a", "b"]);
    shouldBe("\uD83D\uDE00a".match(/(?:\u{1F600}){2}|./u), ["\uD83D\uDE00"]);
    shouldBe("abc".match(/(?:ab){2}|./y), ["a"]);
    shouldBe("xzxza".match(/(?:xz){3}|./), ["x"]);
    shouldBe("xzxzxz".match(/(?:xz){3}|./), ["xzxzxz"]);
    shouldBe("xzxz".match(/(?:xz){2}|./), ["xzxz"]);
    shouldBe("a".match(/(?:xz){2}|./), ["a"]);
    shouldBe("xza".match(/(?:(?:xz){2}|.)/), ["x"]);
    shouldBe("xza".match(/^(?:(?:xz){2}|.)$/), null);
    shouldBe("aBx".match(/(?:(?i:ab)){2}|./), ["a"]);
    shouldBe("xzx".match(/(?:xz){2}|x/g), ["x", "x"]);
    shouldBe("xyza".match(/(?:(xy)z){2}|./), ["x", undefined]);
    shouldBe("xyzxyza".match(/(?:(xy)z){2}|./), ["xyzxyz", "xy"]);
    shouldBe("xzxzxza".match(/(?:xz){4}|(?:xz){3}/), ["xzxzxz"]);
    shouldBe("xzxza".match(/(?:xz){4}|(?:xz){3}|./), ["x"]);

    shouldBe("xzxzb".match(/(?:xz){2}a|./), ["x"]);
    shouldBe("xzxzb".match(/(?:xz){2}a/), null);
    shouldBe("xzxza".match(/(?:xz){2}a|./), ["xzxza"]);
    shouldBe("xzxzxzb".match(/(?:xz){3}a|./), ["x"]);

    shouldBe("xyzxya".match(/(?:(xy)z){2}|./), ["x", undefined]);
    shouldBe("xyzxyz".match(/(?:(xy)z){2}|./), ["xyzxyz", "xy"]);
    shouldBe("xyzxyzb".match(/(?:(xy)z){2}a|./), ["x", undefined]);
    shouldBe("xyzxyza".match(/(?:(xy)z){2}a|./), ["xyzxyza", "xy"]);

    shouldBe("xxa".match(/(?:(?:x){2}){2}|./), ["x"]);
    shouldBe("xxxx".match(/(?:(?:x){2}){2}|./), ["xxxx"]);
    shouldBe("xxxa".match(/(?:(?:x){2}){2}|./), ["x"]);
    shouldBe("xyxya".match(/(?:(?:(x)y){2}){2}|./), ["x", undefined]);

    shouldBe("x".match(/(?:xz){2}|./), ["x"]);
    shouldBe("".match(/(?:xz){2}|./), null);

    shouldBe("xzxzxz".match(/(?:xz){2}|./g), ["xzxz", "x", "z"]);
    shouldBe("xzaxz".match(/(?:xz){2}|./g), ["x", "z", "a", "x", "z"]);
}

{
    let re = /(?:xz){2}|./y;
    for (let i = 0; i < testLoopCount; ++i) {
        re.lastIndex = 1;
        let m = re.exec("axza");
        shouldBe(m, ["x"]);
        shouldBe(re.lastIndex, 2);
    }
}
