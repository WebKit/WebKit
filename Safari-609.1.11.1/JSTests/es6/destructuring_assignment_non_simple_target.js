function test() {
    var msg = {};
    ({
        name: msg.name,
        "parameters": [msg.callback, ...msg["parameters"]],
        0: msg[0]
    } = {
        name: "test",
        parameters: [function cb() { return "test"; }, "a", "b", "c"],
        "0": NaN
    });
    return msg.name === "test" && msg[0] !== msg[0] && msg.callback() === "test" && (msg.parameters + "") === "a,b,c";
}

if (!test())
    throw new Error("Test failed");
