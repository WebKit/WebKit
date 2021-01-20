// This test times out, probably because of the while loop in the agent.
// https://bugs.webkit.org/show_bug.cgi?id=170958
//@ skip

const pageSize = 64 * 1024;

const verbose = false;

// Start multiple agents and create WebAssembly.Memory from each of
// them. Perform writes into each memory, and then check that the memory only
// contains that agent's writes. This would find bugs where an implementation's
// memory reuse is buggy.

// Use the agent support from test262: https://github.com/tc39/test262/blob/master/INTERPRETING.md#host-defined-functions

const testIterations = 2;
const numAgents = 8;
const initialPages = 64;
const numWrites = 1024;

const stateWait = 0;
const stateReady = 1;
const stateCheck = 2;

const startAgents = numAgentsToStart => {
    for (let a = 0; a < numAgentsToStart; ++a) {
        const agentScript = `
        let state = ${stateWait};
        let u8;

        if (${verbose})
            print("Agent ${a} started");
        $.agent.report("Agent ${a} started");

        $.agent.receiveBroadcast((sab, newState) => {
            if (${verbose})
                print("Agent ${a} received broadcast");
            u8 = new Uint8Array(sab);
            state = newState;
        });

        const busyWaitForValue = value => {
            // Busy-wait so that once the SAB write occurs all agents try to create a memory at the same time.
            while (Atomics.load(u8, 0) !== value) ;
        };

        if (${verbose})
            print("Agent ${a} waits");
        $.agent.report("Agent ${a} waits");
        // FIXME: How can this work? The state variable is in the JS heap and the while loop
        // prevents any JS-heap-modifying things from happening because JS is a synchronous
        // language
        // https://bugs.webkit.org/show_bug.cgi?id=170958
        while (state === ${stateWait}) ;
        $.agent.report("Agent ${a} received SAB");
        // Use it for faster state change so all agents are more likely to execute at the same time.
        busyWaitForValue(${stateReady});

        let wasmMemory = new WebAssembly.Memory({ initial: ${initialPages} });
        let memory = new Uint8Array((wasmMemory).buffer);
        if (${verbose})
            print("Agent ${a} performing writes");
        for (let write = 0; write < ${numWrites}; ++write) {
            // Perform writes of our agent number at a random location. This memory should not be shared, if we see writes of other values then something went wrong.
            const idx = (Math.random() * ${pageSize} * ${initialPages}) | 0;
            memory[idx] = ${a};
        }
        if (${verbose})
            print("Agent ${a} writes performed");
        $.agent.report("Agent ${a} performed writes");
        busyWaitForValue(${stateCheck});

        if (${verbose})
            print("Agent ${a} checking");
        // Check that our memory only contains 0 and our agent number.
        for (let idx = 0; idx < ${pageSize} * ${initialPages}; ++idx)
            if (memory[idx] !== 0 && memory[idx] !== ${a})
                throw new Error("Agent ${a} found unexpected value " + memory[idx] + " at location " + idx);
        $.agent.report("Agent ${a} checks out OK");
        $.agent.leaving();
        `;

        if (verbose)
            print(`Starting agent ${a}`);
        $.agent.start(agentScript);
    }
};

const waitForAgents = numAgentsToWaitFor => {
    for (let a = 0; a < numAgentsToWaitFor; ++a) {
        while (true) {
            const report = $.agent.getReport();
            if (report === null) {
                $.agent.sleep(1);
                continue;
            }
            if (verbose)
                print(`Received: ${report}`);
            break;
        }
    }
};

const broadcastToAgents = (sab, newState) => {
    $.agent.broadcast(sab, newState);
};

const sab = new SharedArrayBuffer(1024);
const u8 = new Uint8Array(sab);

for (let it = 0; it < testIterations; ++it) {
    startAgents(numAgents);
    waitForAgents(numAgents);
    broadcastToAgents(sab, stateReady);
    waitForAgents(numAgents);
    $.agent.sleep(1);
    Atomics.store(u8, 0, stateReady);
    waitForAgents(numAgents);
    $.agent.sleep(1);
    Atomics.store(u8, 0, stateCheck);
    waitForAgents(numAgents);
    if (verbose)
        print("Everyting was fine");
    $.agent.sleep(1);
}
