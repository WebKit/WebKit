function test() {

return typeof Math.cbrt === "function";

}

if (!test())
    throw new Error("Test failed");

