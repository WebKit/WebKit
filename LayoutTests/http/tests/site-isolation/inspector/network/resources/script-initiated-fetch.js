// Kept in a separate file so the initiator's top call frame has a real script resource behind it,
// rather than an inline script in the iframe document.
function triggerScriptInitiatedFetch() {
    fetch("data.json?script-initiated-" + Math.random());
}

// Schedules the fetch from a setTimeout callback so the initiator has an async parent stack trace:
// the synchronous stack names fetchFromTimeout, and its parent (captured when setTimeout was called)
// names triggerAsyncScriptInitiatedFetch.
function triggerAsyncScriptInitiatedFetch() {
    setTimeout(function fetchFromTimeout() {
        fetch("data.json?async-script-initiated-" + Math.random());
    }, 0);
}
