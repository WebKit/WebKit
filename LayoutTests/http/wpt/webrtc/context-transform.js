class MockRTCRtpTransformer {
    constructor(transformer) {
        this.askKeyFrame = false;
        this.context = transformer;
        this.context.port.onmessage = (event) => {
            if (event.data === "startKeyFrames")
                this.askKeyFrame = true;
            else if (event.data === "endKeyFrames")
                this.askKeyFrame = false;
        };
        this.start();
    }
    start()
    {
        this.reader = this.context.readable.getReader();
        this.writer = this.context.writable.getWriter();
        this.process();
        this.context.port.postMessage("started " + this.context.options.mediaType + " " + this.context.options.side);
    }

    process()
    {
        this.reader.read().then(chunk => {
            if (chunk.done)
                return;

            this.writer.write(chunk.value);

            if (this.context.options.mediaType === "video") {
                if (chunk.value instanceof RTCEncodedVideoFrame)
                    this.context.port.postMessage("video frame " + chunk.value.type);

                if (this.askKeyFrame)
                    this.context.requestKeyFrame();
            }

            this.process();
        });
    }
};

onrtctransform = (event) => {
    new MockRTCRtpTransformer(event.transformer);
};

self.postMessage("registered");
