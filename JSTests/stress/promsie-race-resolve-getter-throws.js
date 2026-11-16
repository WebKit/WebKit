function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

let count = 0;
Object.defineProperty(Promise, "resolve", {
  get() {
    count++;
    throw new Error("Error from get resolve");
  }
});

let result;
let caught = false;
try {
  result = Promise.race([]);
} catch {
  caught = true;
}

shouldBe(caught, false);
shouldBe(count, 1);
shouldBe(result instanceof Promise, true);
