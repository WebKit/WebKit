function shouldBe(a, b) { if (a !== b) throw new Error(`bad ${a} vs ${b}`); }
const subjects = ["uniform vec3 color;\nvoid main() {}", "abc", "x", "foo bar", "#define X 1"];
for (let iter = 0; iter < testLoopCount; ++iter) {
    for (const s of subjects) {
        const r = s.split("#include<nothing>");
        shouldBe(r.length, 1);
        shouldBe(r[0], s);
        shouldBe(r.indexOf(s), 0);
        shouldBe(r.includes(s), true);
        const r2 = s.split(" ");
        shouldBe(r2.join(" "), s);
    }
}
const dyn = ("uniform" + " vec3").split("zz");
shouldBe(dyn[0], "uniform vec3");
