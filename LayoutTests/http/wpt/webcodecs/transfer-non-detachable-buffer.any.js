// META: global=window,dedicatedworker

// A WebAssembly.Memory buffer is pinned, so it can never be detached and cannot be transferred.
function nonDetachableBuffer()
{
    return new WebAssembly.Memory({ initial: 1 }).buffer;
}

test(() => {
    const transferred = nonDetachableBuffer();
    const data = new Uint8Array([1, 2, 3, 4]);

    assert_throws_js(TypeError, () => {
        new EncodedAudioChunk({ type: 'key', timestamp: 0, data: data, transfer: [transferred] });
    });

    assert_equals(transferred.byteLength, 65536, 'transfer list buffer is not detached');
    assert_equals(data.buffer.byteLength, 4, 'data buffer is not detached');
}, 'EncodedAudioChunk rejects a non-detachable buffer in transfer');

test(() => {
    const transferred = nonDetachableBuffer();
    const data = new Uint8Array([1, 2, 3, 4]);

    assert_throws_js(TypeError, () => {
        new EncodedVideoChunk({ type: 'key', timestamp: 0, data: data, transfer: [transferred] });
    });

    assert_equals(transferred.byteLength, 65536, 'transfer list buffer is not detached');
    assert_equals(data.buffer.byteLength, 4, 'data buffer is not detached');
}, 'EncodedVideoChunk rejects a non-detachable buffer in transfer');

test(() => {
    const transferred = nonDetachableBuffer();
    const data = new Float32Array(10);

    assert_throws_js(TypeError, () => {
        new AudioData({
            timestamp: 0,
            data: data,
            numberOfChannels: 1,
            numberOfFrames: 10,
            sampleRate: 48000,
            format: 'f32',
            transfer: [transferred]
        });
    });

    assert_equals(transferred.byteLength, 65536, 'transfer list buffer is not detached');
    assert_equals(data.buffer.byteLength, 40, 'data buffer is not detached');
}, 'AudioData rejects a non-detachable buffer in transfer');
