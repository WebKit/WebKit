// Web Audio order for 5.1 (https://webaudio.github.io/web-audio-api/#ChannelOrdering), with the tone
// carried by each speaker of the resources/media/*-5.1 files.
const expectedOrder = ['FL', 'FR', 'FC', 'LFE', 'BL', 'BR'];
const tones = { FL: 500, FR: 700, FC: 900, LFE: 100, BL: 1100, BR: 1300 };

// Returns the speaker label whose tone dominates samples, or '?' if none clearly does.
function identifyChannel(samples, sampleRate)
{
    // Skip encoder priming and analyse 100ms.
    const start = 4800;
    const length = 4800;
    if (samples.length < start + length)
        return '?';
    const powers = Object.entries(tones).map(([label, frequency]) => {
        const w = 2 * Math.PI * frequency / sampleRate;
        let re = 0, im = 0;
        for (let i = 0; i < length; i++) {
            re += samples[start + i] * Math.cos(w * i);
            im += samples[start + i] * Math.sin(w * i);
        }
        return { label, power: re * re + im * im };
    }).sort((a, b) => b.power - a.power);
    return powers[0].power > 100 * powers[1].power ? powers[0].label : '?';
}

function checkChannelOrder(buffer)
{
    if (buffer.numberOfChannels !== expectedOrder.length) {
        testFailed("Decoding returned the wrong number of channels: " + buffer.numberOfChannels);
        return;
    }
    testPassed("Decoding returned 6 channels.");
    const order = [];
    for (let channel = 0; channel < buffer.numberOfChannels; channel++)
        order.push(identifyChannel(buffer.getChannelData(channel), buffer.sampleRate));
    if (order.join() === expectedOrder.join())
        testPassed("Channels are in Web Audio order: " + order.join());
    else
        testFailed("Channels are in the wrong order: " + order.join());
}
