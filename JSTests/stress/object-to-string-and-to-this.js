function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const mathToString = Math.toString;

function foo() {
  return mathToString();
}

let before = foo();
for (let i = 0; i < testLoopCount; i++) {
  foo();
}
let after = foo();

shouldBe(before, after);
