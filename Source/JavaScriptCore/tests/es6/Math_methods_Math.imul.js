function test() {

return typeof Math.imul === "function";

}

if (!test())
    throw new Error("Test failed");

