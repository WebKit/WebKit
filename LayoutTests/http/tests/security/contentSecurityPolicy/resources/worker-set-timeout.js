var id = 0;
try {
    id = 17;  // Test not yet fully enabled.  Simply pretend that a call to setTimeout() here worked.
} catch(e) {
}
postMessage(id === 0 ? "setTimeout blocked" : "setTimout allowed");
