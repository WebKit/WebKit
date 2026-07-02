function test() {

return typeof Math.sinh === "function";

}

if (!test())
    throw new Error("Test failed");

