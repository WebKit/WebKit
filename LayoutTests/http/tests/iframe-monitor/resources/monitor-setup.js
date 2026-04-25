async function setup() {
    if (window.testRunner && 'canModifyResourceMonitorList' in testRunner && testRunner.canModifyResourceMonitorList) {
        const rules = [
            rule('--eligible--'),
        ];

        await testRunner.setResourceMonitorList(JSON.stringify(rules));

        // Lower the threshold to 10k
        internals.setResourceMonitorNetworkUsageThreshold(10 * 1024, 0.001);

        // Skip throttling of unloading or not.
        internals.shouldSkipResourceMonitorThrottling = false;

        return true;
    } else {
        console.error('ResourceMonitor is not available or cannot modify rules.');
        finishJSTest();
        return false;
    }
}

function afterSetup(action) {
    return async () => {
        const result = await setup();
        console.log(`setup() finished: ${result}`);
        if (result) {
            try {
                await action();
            } catch (e) {
                console.error(`error on action: ${e}`);
                finishJSTest();
            }
        }
    };
}

function rule(filter, action = 'block') {
    const trigger = { "url-filter": filter };

    return { action: { type: action }, trigger };
}

async function pause(ms) {
    return new Promise((resolve) => {
        setTimeout(() => resolve(), ms);
    });
}

async function waitUntilUnload(name) {
    const iframe = document.querySelector(`iframe[name=${name}]`);
    if (!iframe)
        throw new Error("iframe dosn't exist");

    while (!iframe.srcdoc) {
        await pause(10);
    }

    // extra wait time
    await pause(100);
    return iframe;
}
