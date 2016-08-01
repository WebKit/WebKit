function test() {

return typeof Math.cosh === "function";

}

if (!test())
    throw new Error("Test failed");

