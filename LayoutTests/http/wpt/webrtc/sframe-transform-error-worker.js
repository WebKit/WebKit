let triggerError;
onmessage = (event) => triggerError = event.data.action;

onrtctransform = async (event) => {
    const transformer = event.transformer;

    const key = await crypto.subtle.importKey("raw", new Uint8Array([143, 77, 43, 10, 72, 19, 37, 67, 236, 219, 24, 93, 26, 165, 91, 178]), "HKDF", false, ["deriveBits", "deriveKey"]);
    const sframeTransform = new SFrameTransform({ role: "encrypt", authenticationSize: "4" });
    sframeTransform.setEncryptionKey(key, 1);

    const postTransform = new TransformStream({transform: (frame, controller) => {
       if (triggerError === "authenticationError") {
          const data = new Uint8Array(frame.data);
          // We change the authentication tag.
          data[data.byteLength - 1] = !data[data.byteLength - 1];
          frame.data = data.buffer;
       } else if (triggerError === "syntaxError") {
          const data = new Uint8Array(frame.data);
          // We set the signature bit.
          data[0] = 255;
          frame.data = data.buffer;
       }
       triggerError = undefined;
       controller.enqueue(frame);
    }});

    transformer.readable.pipeThrough(sframeTransform)
		.pipeThrough(postTransform)
		.pipeTo(transformer.writable);
}
self.postMessage("registered");

