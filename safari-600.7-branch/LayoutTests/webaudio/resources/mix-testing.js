var toneLengthSeconds = 1;

// Create a buffer with multiple channels.
// The signal frequency in each channel is the multiple of that in the first channel.
function createToneBuffer(context, frequency, duration, numberOfChannels) {
    var sampleRate = context.sampleRate;
    var sampleFrameLength = duration * sampleRate;
    
    var audioBuffer = context.createBuffer(numberOfChannels, sampleFrameLength, sampleRate);

    var n = audioBuffer.length;

    for (var k = 0; k < numberOfChannels; ++k)
    {
        var data = audioBuffer.getChannelData(k);

        for (var i = 0; i < n; ++i)
            data[i] = Math.sin(frequency * (k + 1) * 2.0*Math.PI * i / sampleRate);
    }

    return audioBuffer;
}

