function shouldBe(actual, expected) {
  if (actual !== expected)
    throw new Error(`Bad value: ${actual}!`);
}

async function instanceFieldTest() {
  class C {
    [await Promise.resolve(1)] = 1;
  }

  return (new C)[await Promise.resolve(1)];
}

async function* staticFieldTest() {
  class C {
    static [await Promise.resolve(2)] = 2;
  }

  return C[await Promise.resolve(2)];
}

let instanceFieldValue;
instanceFieldTest().then(value => {
  instanceFieldValue = value;
});

let staticFieldValue;
staticFieldTest().next().then(({value}) => {
  staticFieldValue = value;
});

drainMicrotasks();

shouldBe(instanceFieldValue, 1);
shouldBe(staticFieldValue, 2);
