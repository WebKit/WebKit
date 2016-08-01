function test() {
  var result0, result1, i = 0;
  ({ [i++]: result0 = "hungryByDefault", [i++]: result1 = "hippoByDefault"} = []);
  return result0 === "hungryByDefault" && result1 === "hippoByDefault" && i === 2;
}

if (!test())
  throw new Error("Test failed");
