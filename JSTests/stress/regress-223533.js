function shouldBe(actual, expected) {
  if (actual !== expected)
    throw new Error(`Bad value: ${actual}!`);
}

function* test1(foo) {
  shouldBe(foo, 1);
  var foo;
  shouldBe(foo, 1);
}

function* test2(...foo) {
  shouldBe(`${foo}`, "2,2,2");
  var foo = 2;
  shouldBe(foo, 2);
}

async function test3([foo]) {
  shouldBe(foo, 3);
  var foo;
  shouldBe(foo, 3);
}

async function test4(foo) {
  shouldBe(foo, 4);
  var foo;
  shouldBe(foo, 4);
}

async function* test5({foo}) {
  shouldBe(foo, 5);
  var foo = 5;
  shouldBe(foo, 5);
}

function* hoistingTest1(foo) {
  shouldBe(foo, 1);
  {
    function foo() { return "hoisted"; }
  }
  shouldBe(foo, 1);
}

async function* hoistingTest2([foo]) {
  shouldBe(foo, 2);
  if (true) {
    function foo() { return "hoisted"; }
  }
  shouldBe(foo, 2);
}

(async function() {
  try {
    for (let _ of test1(1)) {}
    for (let _ of test2(2, 2, 2)) {}
    await test3([3]);
    await test4(4);
    for await (let _ of test5({foo: 5})) {}

    for (let _ of hoistingTest1(1)) {}
    for await (let _ of hoistingTest2([2])) {}
  } catch (error) {
    print(`FAIL! ${error}\n${error.stack}`);
    $vm.abort();
  }
})();

drainMicrotasks();
