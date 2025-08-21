function waitFor(object, event) {
    return new Promise(resolve => {
        object.addEventListener(event, resolve, {once: true});
    })
}

function sleepFor(timeout) {
    return new Promise(resolve => setTimeout(resolve, timeout));
}
