function test() {

return typeof Math.fround === "function";

}

if (!test())
    throw new Error("Test failed");

