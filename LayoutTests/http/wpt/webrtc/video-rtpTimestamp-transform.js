class MockRTCRtpTransformer {
    constructor(transformer) {
        this.validateRTPTimestamp = false;
        this.startTime = undefined;
        this.context = transformer;
        this.context.options.port.onmessage = (event) => {
            if (event.data === "validateRTPTimestamp")
                this.validateRTPTimestamp = true;
        };
        this.start();
    }
    start()
    {
        this.reader = this.context.readable.getReader();
        this.writer = this.context.writable.getWriter();
        this.process();
        this.context.options.port.postMessage("started " + this.context.options.mediaType + " " + this.context.options.side);
    }

    process()
    {
        this.reader.read().then(chunk => {
            if (chunk.done)
                return;

            if (this.validateRTPTimestamp) {
                if (!this.startTime) {
                    this.startTime = Date.now();
                    this.startRTPTimestamp = chunk.value.getMetadata().rtpTimestamp;
                }
                const currentDelay = Date.now() - this.startTime;
                if (currentDelay >= 1000) {
                    const rtpTimestampDelta = chunk.value.getMetadata().rtpTimestamp - this.startRTPTimestamp;
                    const frequency = rtpTimestampDelta / currentDelay;
                    this.context.options.port.postMessage(frequency > 40 && frequency < 140 ? "PASS" : ("FAIL with delta " + rtpTimestampDelta));
                    this.validateRTPTimestamp = false;
                    this.startTime = undefined;
                }
            }

            this.writer.write(chunk.value);
            this.process();
        });
    }
};

onrtctransform = (event) => {
    new MockRTCRtpTransformer(event.transformer);
};

self.postMessage("registered");
