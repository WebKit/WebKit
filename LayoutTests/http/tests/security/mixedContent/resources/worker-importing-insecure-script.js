try {
    importScripts("http://127.0.0.1:8000/security/mixedContent/resources/worker-sending-message.js");
} catch (error) {
    postMessage("ERROR");
}
