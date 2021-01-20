class SFrameRTCRtpTransformer extends RTCRtpScriptTransformer {
    constructor() {
        super();
        this.sframeTransform = new SFrameTransform({ role : "decrypt", authenticationSize: "10", compatibilityMode: "H264" });
        crypto.subtle.importKey("raw", new Uint8Array([143, 77, 43, 10, 72, 19, 37, 67, 236, 219, 24, 93, 26, 165, 91, 178]), "HKDF", false, ["deriveBits", "deriveKey"]).then(key => this.sframeTransform.setEncryptionKey(key));
    }
    start(readableStream, writableStream)
    {
        readableStream.pipeThrough(this.sframeTransform).pipeTo(writableStream);
    }
};

registerRTCRtpScriptTransformer("SFrameRTCRtpTransform", SFrameRTCRtpTransformer);
self.postMessage("registered");
