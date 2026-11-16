//@ requireOptions("--useWasmJSTypes=1")
//@ skip if $addressBits <= 32
import * as assert from "../assert.js";

// A shared memory handed to another agent has to arrive with the address type it was created with.
// Nothing about the shared contents records it, and a memory declared with a maximum of zero has no
// shared contents at all.

const agentSource = `
$.agent.receiveBroadcast(function (memory) {
    let report = [];
    report.push("address=" + memory.type().address);
    report.push("minimum=" + typeof memory.type().minimum);
    try {
        report.push("grow(0n)=" + typeof memory.grow(0n));
    } catch (error) {
        report.push("grow(0n) threw " + error.name);
    }
    $.agent.report(report.join(" "));
    $.agent.leaving();
});
`;

function broadcastAndCollect(memory) {
    $.agent.start(agentSource);
    $.agent.broadcast(memory);
    let report = null;
    while ((report = $.agent.getReport()) === null)
        $.agent.sleep(1);
    return report;
}

assert.eq(broadcastAndCollect(new WebAssembly.Memory({ address: "i64", initial: 1n, maximum: 4n, shared: true })),
    "address=i64 minimum=bigint grow(0n)=bigint");

// Zero maximum: there are no shared contents for the address type to travel with.
assert.eq(broadcastAndCollect(new WebAssembly.Memory({ address: "i64", initial: 0n, maximum: 0n, shared: true })),
    "address=i64 minimum=bigint grow(0n)=bigint");

assert.eq(broadcastAndCollect(new WebAssembly.Memory({ initial: 1, maximum: 4, shared: true })),
    "address=i32 minimum=number grow(0n) threw TypeError");
