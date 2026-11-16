//@ memoryHog!
//@ runDefault("--useFuzzerMode=1")
// In fuzzer mode, $.agent.start throws once 64 agents are alive, and succeeds again after they exit.

const maxLiveAgents = 64;
for (let i = 0; i < maxLiveAgents; i++)
    $.agent.start(`$.agent.receiveBroadcast(() => { });`);

let error;
try {
    $.agent.start(``);
} catch (e) {
    error = e;
}
if (!(error instanceof Error) || error.message !== "Too many live agents")
    throw new Error(`Expected "Too many live agents" error, got: ${error}`);

$.agent.broadcast(new SharedArrayBuffer(4), 0);

for (let attempt = 0; ; attempt++) {
    try {
        $.agent.start(``);
        break;
    } catch (e) {
        if (attempt === 1000)
            throw e;
        $.agent.sleep(10);
    }
}
