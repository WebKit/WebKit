class AudioVideoRTCRtpTransformer {
    constructor(transformer) {
        this.askKeyFrame = false;
        this.context = transformer;
        this.context.options.port.onmessage = (event) => {
            if (event.data === "tryGenerateKeyFrame")
                this.tryGenerateKeyFrame = true;
            else if (event.data === "trySendKeyFrameRequest")
                this.trySendKeyFrameRequest = true;
            else if (event.data === "tryGenerateKeyFramePromiseHandling")
                this.tryGenerateKeyFramePromiseHandling = true;
            else if (event.data === "tryAccessingDataTwice")
                this.tryAccessingDataTwice = true;
        };
        this.start();
    }
    start()
    {
        this.reader = this.context.readable.getReader();
        this.writer = this.context.writable.getWriter();
        this.process();
    }

    process()
    {
        this.reader.read().then(chunk => {
            if (chunk.done)
                return;

            if (this.tryAccessingDataTwice) {
                this.tryAccessingDataTwice = false;
                const data1 = chunk.value.data;
                const data2 = chunk.value.data;
                this.context.options.port.postMessage(data1 === data2 ? "PASS" : "FAIL, data objects are different");
            }

            this.writer.write(chunk.value);

            if (this.tryGenerateKeyFrame) {
                this.tryGenerateKeyFrame = false;
                this.context.generateKeyFrame().then(() => {
                    this.context.options.port.postMessage("PASS");
                }, (e) => {
                    this.context.options.port.postMessage("FAIL: " + e.name);
                });
            }
            if (this.trySendKeyFrameRequest) {
                this.trySendKeyFrameRequest = false;
                this.context.sendKeyFrameRequest().then(() => {
                    this.context.options.port.postMessage("PASS");
                }, (e) => {
                    this.context.options.port.postMessage("FAIL: " + e.name);
                });
            }
            if (this.tryGenerateKeyFramePromiseHandling) {
                this.tryGenerateKeyFramePromiseHandling = false;
                let test1 = false, test2 = false, test3 = false;
                let result; 
                this.context.generateKeyFrame().then(() => {
                    test1 = true;
                    if (test2 || test3)
                        result = 'error 1';
                }, (e) => {console.log(e);});
                this.context.generateKeyFrame().then(() => {
                    test2 = true;
                    if (!test1 || test3)
                        result = 'error 2';
                }).then(() => {
                    if (!test3)
                        result = 'error 3';
                    this.context.options.port.postMessage(!result ? "PASS" : "FAIL: " + result);
                });
                this.context.generateKeyFrame().then(() => {
                    test3 = true;
                    if (!test1 || !test2)
                        result = 'error 4';
                });
            }
            this.process();
        });
    }
};

onrtctransform = (event) => {
    new AudioVideoRTCRtpTransformer(event.transformer);
};

self.postMessage("registered");
