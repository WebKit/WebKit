function test() {
  var result0, result1, i = 0;
  ({ [i++]: result0, [i++]: result1 } = ["hungryhungry", "hippos"]);
  return result0 === "hungryhungry" && result1 === "hippos" && i === 2;
}

if (!test())
  throw new Error("Test failed");
