promise_test(t => {
    let resolveCallback, rejectCallback;
    const promise = new Promise((resolve, reject) => {
        resolveCallback = resolve;
        rejectCallback = reject;
    });
    const decoder = new VideoDecoder({
        output(frame) {
            assert_equals(frame.codedWidth, 268, "width");
            assert_equals(frame.codedHeight, 268, "height");

            frame.close();
            resolveCallback();
        },
        error(e) {
            rejectCallback(e);
        }
    });

    decoder.configure({
        codec: "hev1.1.6.L90.90",
        hardwareAcceleration: "no-preference"
    });

    const data = new Uint8Array([0, 0, 0, 1, 64, 1, 12, 1, 255, 255, 1, 96, 0, 0, 3, 0, 144, 0, 0, 3, 0, 0, 3, 0, 60, 149, 152, 9, 0, 0, 0, 1, 66, 1, 1, 1, 96, 0, 0, 3, 0, 144, 0, 0, 3, 0, 0, 3, 0, 60, 160, 8, 136, 4, 71, 119, 150, 86, 105, 36, 202, 230, 128, 128, 0, 0, 3, 0, 128, 0, 0, 3, 0, 132, 0, 0, 0, 1, 68, 1, 193, 114, 180, 98, 64, 0, 0, 1, 40, 1, 175, 19, 41, 104, 147, 158, 230, 105, 128, 191, 255, 233, 135, 63, 31, 217, 0, 181, 98, 99, 182, 187, 224, 9, 0, 128, 128, 165, 239, 114, 184, 5, 192, 31, 34, 97, 21, 147, 2, 132, 42, 64, 0, 12, 104, 135, 0, 0, 3, 0, 0, 91, 64, 0, 0, 3, 0, 2, 182 ]);
    decoder.decode(new EncodedVideoChunk({
        timestamp: 1,
        type: "key",
        data,
    }));
    decoder.flush();
    return promise;
}, "Test that HEVC decoder with annexb data");
