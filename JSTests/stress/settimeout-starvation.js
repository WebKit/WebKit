let promise = new Promise((resolve, _) => {
    setTimeout(() => { resolve("timed-out") }, 1);
});
let outcome = null;

(function wait() {
    if (outcome === "timed-out") {
        return;
    }
    setTimeout(wait, 0);
})();

promise.then(result => { outcome = result; });
