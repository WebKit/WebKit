//@ memoryHog!
//@ runDefault("--collectContinuously=1", "--collectContinuouslyPeriodMS=0.1")

// Destroys many agent VMs while each one's collector thread is collecting continuously. Agents start
// other agents so that hundreds of VMs are alive and being torn down at once, and every agent's pending
// timers keep the process-wide JSRunLoopTimer state busy while those VMs are destroyed.

const rootAgents = 16;
const leafAgentsPerRoot = 32;
const durationMS = 40;

const leafSource = `
    let values = [];
    for (let i = 0; i < 50; i++)
        setTimeout(() => { values.push(i); }, i);
`;

const rootSource = `
    const start = $.agent.monotonicNow();
    for (let i = 0; i < ${leafAgentsPerRoot} && $.agent.monotonicNow() - start < ${durationMS}; i++)
        $.agent.start(${JSON.stringify(leafSource)});
`;

for (let i = 0; i < rootAgents; i++)
    $.agent.start(rootSource);
$.agent.sleep(durationMS);
