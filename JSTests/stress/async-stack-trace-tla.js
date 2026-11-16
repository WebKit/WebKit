//@ requireOptions("--useAsyncStackTrace=1", "-m")

function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

async function foo(x) {
  await bar(x);
}
async function bar(x) {
  await x;
  throw new Error("FOO");
}

try {
  await foo();
} catch (e) {
  const lines = e.stack.split("\n");
  shouldBe(lines.length, 2);
  shouldBe(lines[0].slice(0, 4), "bar@");
  shouldBe(lines[1].slice(0, 10), "async foo@");
}
