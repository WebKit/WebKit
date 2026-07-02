function test() {

return typeof Math.acosh === "function";

}

if (!test())
    throw new Error("Test failed");

