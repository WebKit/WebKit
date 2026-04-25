function test() {

return typeof Math.log1p === "function";

}

if (!test())
    throw new Error("Test failed");

