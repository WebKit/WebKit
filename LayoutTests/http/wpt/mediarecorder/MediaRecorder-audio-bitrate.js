async function record(stream, bitRate, mimeType)
{
    let blob;
    let type;
    let count = 0;
    const recorder = new MediaRecorder(stream, { mimeType : mimeType, audioBitsPerSecond : bitRate });
    const promise = new Promise((resolve, reject) => {
        recorder.ondataavailable = (e) => {
             if (!blob) {
                 blob = e.data;
                 type = e.data.type;
             } else
                 blob = new Blob([blob, e.data]);
             ++count;
             if (blob.size < 500)
                 return;
             resolve(blob);
        };
        setTimeout(() => reject("not recording enough data, blob count " + count + ", size is " + (blob?.size)), 15000);
    });
    recorder.start();
    const interval = setInterval(() => recorder.requestData(), 500);
    await promise;
    clearInterval(interval);
    recorder.stop();
    return new Promise((resolve, reject) => {
        recorder.ondataavailable = e => resolve(new Blob([blob, e.data], { type }));
        setTimeout(() => reject("blob event when stopping timed out"), 5000);
    });
}
