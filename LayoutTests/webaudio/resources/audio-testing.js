if (window.layoutTestController)
    layoutTestController.overridePreference("WebKitWebAudioEnabled", "1");

function writeString(s, a, offset) {
    for (var i = 0; i < s.length; ++i) {
        a[offset + i] = s.charCodeAt(i);
    }
}

function writeInt16(n, a, offset) {
    n = Math.floor(n);
    
    var b1 = n & 255;
    var b2 = (n >> 8) & 255;

    a[offset + 0] = b1;
    a[offset + 1] = b2;
}

function writeInt32(n, a, offset) {
    n = Math.floor(n);
    var b1 = n & 255;
    var b2 = (n >> 8) & 255;
    var b3 = (n >> 16) & 255;
    var b4 = (n >> 24) & 255;

    a[offset + 0] = b1;
    a[offset + 1] = b2;
    a[offset + 2] = b3;
    a[offset + 3] = b4;
}

function writeAudioBuffer(audioBuffer, a, offset) {
    var n = audioBuffer.length;
    var bufferL = audioBuffer.getChannelData(0);
    var bufferR = audioBuffer.getChannelData(1);
    
    for (var i = 0; i < n; ++i) {
        // Write left and right
        var sampleL = bufferL[i] * 32768.0;
        var sampleR = bufferR[i] * 32768.0;

        // Clip left and right samples to the limitations of 16-bit.
        // If we don't do this then we'll get nasty wrap-around distortion.
        if (sampleL < -32768)
            sampleL = -32768;
        if (sampleL > 32767)
            sampleL = 32767;
        if (sampleR < -32768)
            sampleR = -32768;
        if (sampleR > 32767)
            sampleR = 32767;
            
        writeInt16(sampleL, a, offset);
        writeInt16(sampleR, a, offset + 2);
        offset += 4;
    }
}

function createWaveFileData(audioBuffer) {
    var frameLength = audioBuffer.length;
    var numberOfChannels = audioBuffer.numberOfChannels;
    var sampleRate = audioBuffer.sampleRate;
    var bitsPerSample = 16;
    var byteRate = sampleRate * numberOfChannels * bitsPerSample/8;
    var blockAlign = numberOfChannels * bitsPerSample/8;
    var wavDataByteLength = frameLength * numberOfChannels * 2; // 16-bit audio
    var headerByteLength = 44;
    var totalLength = headerByteLength + wavDataByteLength;

    var waveFileData = new Uint8Array(totalLength);
    
    var subChunk1Size = 16; // for linear PCM
    var subChunk2Size = wavDataByteLength;
    var chunkSize = 4 + (8 + subChunk1Size) + (8 + subChunk2Size);

    writeString("RIFF", waveFileData, 0);
    writeInt32(chunkSize, waveFileData, 4);
    writeString("WAVE", waveFileData, 8);
    writeString("fmt ", waveFileData, 12);
    
    writeInt32(subChunk1Size, waveFileData, 16);      // SubChunk1Size (4)
    writeInt16(1, waveFileData, 20);                  // AudioFormat (2)
    writeInt16(numberOfChannels, waveFileData, 22);   // NumChannels (2)
    writeInt32(sampleRate, waveFileData, 24);         // SampleRate (4)
    writeInt32(byteRate, waveFileData, 28);           // ByteRate (4)
    writeInt16(blockAlign, waveFileData, 32);         // BlockAlign (2)
    writeInt32(bitsPerSample, waveFileData, 34);      // BitsPerSample (4)
                                                      
    writeString("data", waveFileData, 36);            
    writeInt32(subChunk2Size, waveFileData, 40);      // SubChunk2Size (4)
    
    // Write actual audio data starting at offset 44.
    writeAudioBuffer(audioBuffer, waveFileData, 44);
    
    return waveFileData;
}

function createAudioData(audioBuffer) {
    return createWaveFileData(audioBuffer);
}

function finishAudioTest(event) {
    var audioData = createAudioData(event.renderedBuffer);
    layoutTestController.setAudioData(audioData);
    layoutTestController.notifyDone();
}

// Create an impulse in a buffer of length sampleFrameLength
function createImpulseBuffer(context, sampleFrameLength) {
    var audioBuffer = context.createBuffer(1, sampleFrameLength, context.sampleRate);
    var n = audioBuffer.length;
    var dataL = audioBuffer.getChannelData(0);

    for (var k = 0; k < n; ++k) {
        dataL[k] = 0;
    }
    dataL[0] = 1;

    return audioBuffer;
}

// Convert time (in seconds) to sample frames.
function timeToSampleFrame(time, sampleRate) {
    return Math.floor(0.5 + time * sampleRate);
}

// Compute the number of sample frames consumed by noteGrainOn with
// the specified |grainOffset|, |duration|, and |sampleRate|.
function grainLengthInSampleFrames(grainOffset, duration, sampleRate) {
    var startFrame = timeToSampleFrame(grainOffset, sampleRate);
    var endFrame = timeToSampleFrame(grainOffset + duration, sampleRate);

    return endFrame - startFrame;
}
