
(async () => {
    if (!window.testRunner)
        return;

    testRunner.waitUntilDone();

    const script = document.createElement("script");
    script.src = "../resources/ui-helper.js";
    script.addEventListener("load", async event => {
        await UIHelper.ensureStablePresentationUpdate();
        testRunner.notifyDone();
    });
    document.body.appendChild(script);
})();
