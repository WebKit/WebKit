
(async () => {
    window.testRunner?.waitUntilDone();

    await Promise.all(document.getAnimations().map(animation => animation.ready));

    await new Promise(requestAnimationFrame);
    await new Promise(requestAnimationFrame);
    await new Promise(requestAnimationFrame);

    window.testRunner?.notifyDone();
})();
