function record(stream, bitRate, mimeType)
{
    const recorder = new MediaRecorder(stream, { mimeType : mimeType, audioBitsPerSecond : bitRate });
    const promise = new Promise((resolve, reject) => {
        recorder.ondataavailable = (e) => resolve(e.data);
        setTimeout(reject, 5000);
    });
    recorder.start();
    setTimeout(() => recorder.stop(), 2500);
    return promise;
}
