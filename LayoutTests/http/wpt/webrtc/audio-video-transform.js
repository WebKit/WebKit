var audioSenderTransformer, videoSenderTransformer;
var audioReceiverTransformer, videoReceiverTransformer;
var audioChunk, videoChunk;

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
            else if (event.data === "checkDataAfterWrite")
                this.checkDataAfterWrite = true;
            else if (event.data === "checkModifiedDataAfterWrite")
                this.checkModifiedDataAfterWrite = true;
            else if (event.data === "tryAccessingDataTwice")
                this.tryAccessingDataTwice = true;
            else if (event.data === "tryAccessingMetadata")
                this.tryAccessingMetadata = true;
            else if (event.data === "tryWritingAudio")
                this.tryWritingAudio = true;
            else if (event.data === "tryWritingVideo")
                this.tryWritingVideo = true;
        };

        if (this.context.options.side === "sender") {
            if (this.context.options.mediaType === "audio")
                audioSenderTransformer = this;
            else if (this.context.options.mediaType === "video")
                videoSenderTransformer = this;
        } else {
            if (this.context.options.mediaType === "audio")
                audioReceiverTransformer = this;
            else if (this.context.options.mediaType === "video")
                videoReceiverTransformer = this;
        }

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
        this.reader.read().then(async chunk => {
            if (chunk.done)
                return;

            if (audioSenderTransformer && audioSenderTransformer.tryWritingVideo) {
                if (audioSenderTransformer === this) {
                    this.writer.write(chunk.value);
                    if (videoChunk !== undefined) {
                       this.writer.write(videoChunk.value);
                       audioSenderTransformer.tryWritingVideo = false;
                       this.context.options.port.postMessage("PASS");
                    }
                    this.process();
                    return;
                }
                if(videoSenderTransformer === this) {
                    videoChunk = chunk;
                    while (audioSenderTransformer.tryWritingVideo)
                        await new Promise(resolve => setTimeout(resolve, 50));
                    videoSenderTransformer.writer.write(videoChunk);
                    videoChunk = undefined;
                    this.process();
                    return;
                }
            }

            if (videoSenderTransformer && videoSenderTransformer.tryWritingAudio) {
                if (videoSenderTransformer === this) {
                    this.writer.write(chunk.value);
                    if (audioChunk !== undefined) {
                       this.writer.write(audioChunk.value);
                       videoSenderTransformer.tryWritingAudio = false;
                       this.context.options.port.postMessage("PASS");
                    }
                    this.process();
                    return;
                }
                if(audioSenderTransformer === this) {
                    audioChunk = chunk;
                    while (videoSenderTransformer.tryWritingAudio)
                        await new Promise(resolve => setTimeout(resolve, 50));
                    audioSenderTransformer.writer.write(audioChunk);
                    audioChunk = undefined;
                    this.process();
                    return;
                }
            }

            if (audioSenderTransformer && audioSenderTransformer.tryWritingAudio) {
                if (audioSenderTransformer === this) {
                    this.writer.write(chunk.value);
                    if (audioChunk !== undefined) {
                       this.writer.write(audioChunk.value);
                       audioSenderTransformer.tryWritingAudio = false;
                       this.context.options.port.postMessage("PASS");
                    }
                    this.process();
                    return;
                }
                if(audioReceiverTransformer === this) {
                    audioChunk = chunk;
                    while (audioSenderTransformer.tryWritingAudio)
                        await new Promise(resolve => setTimeout(resolve, 50));
                    audioReceiverTransformer.writer.write(audioChunk);
                    audioChunk = undefined;
                    this.process();
                    return;
                }
            }

            if (videoSenderTransformer && videoSenderTransformer.tryWritingVideo) {
                if (videoSenderTransformer === this) {
                    this.writer.write(chunk.value);
                    if (videoChunk !== undefined) {
                       this.writer.write(videoChunk.value);
                       videoSenderTransformer.tryWritingVideo = false;
                       this.context.options.port.postMessage("PASS");
                    }
                    this.process();
                    return;
                }
                if (videoReceiverTransformer === this) {
                    videoChunk = chunk;
                    while (videoSenderTransformer.tryWritingVideo)
                        await new Promise(resolve => setTimeout(resolve, 50));
                    videoReceiverTransformer.writer.write(videoChunk);
                    videoChunk = undefined;
                    this.process();
                    return;
                }
            }

            if (this.tryAccessingDataTwice) {
                this.tryAccessingDataTwice = false;
                const data1 = chunk.value.data;
                const data2 = chunk.value.data;
                this.context.options.port.postMessage(data1 === data2 ? "PASS" : "FAIL, data objects are different");
            }

            if (this.tryAccessingMetadata) {
                this.tryAccessingMetadata = false;
                this.context.options.port.postMessage(chunk.value.getMetadata());
            }

           let valueData = null;
           if (this.checkModifiedDataAfterWrite)
               valueData = structuredClone(chunk.value.data);

            this.writer.write(chunk.value);

            if (this.checkDataAfterWrite) {
                this.checkDataAfterWrite = false;
                this.context.options.port.postMessage(!chunk.value.length ? "PASS" : "FAIL");
            }

            if (this.checkModifiedDataAfterWrite) {
                this.checkModifiedDataAfterWrite = false;
                this.context.options.port.postMessage((!chunk.value.length && !valueData.length) ? "PASS" : "FAIL");
            }


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
