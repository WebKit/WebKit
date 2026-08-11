const BT601 = { primaries: "bt470bg", transfer: "bt709", matrix: "bt470bg", fullRange: false };
const BT709 = { primaries: "bt709",   transfer: "bt709", matrix: "bt709",   fullRange: false };

function makeI420Frame(width, height, y, u, v, timestamp, colorSpace) {
    const yPlaneSize = width * height;
    const uvPlaneSize = ((width + 1) / 2) * ((height + 1)/ 2);
    const buffer = new Uint8Array(yPlaneSize + 2 * uvPlaneSize);
    buffer.fill(y, 0, yPlaneSize);
    buffer.fill(u, yPlaneSize, yPlaneSize + uvPlaneSize);
    buffer.fill(v, yPlaneSize + uvPlaneSize);
    return new VideoFrame(buffer, {
        format: "I420",
        codedWidth: width,
        codedHeight: height,
        timestamp,
        colorSpace,
    });
}

// Mutable per-generator color space — `swap` flips the two slots so subsequent
// frames carry the swapped tags. Index matches the corresponding generator /
// writer / receiver-track index.
const senderColorSpaces = [BT601, BT709];

const generators = [new VideoTrackGenerator(), new VideoTrackGenerator()];
const writers = generators.map(g => g.writable.getWriter());

// Hand both tracks to main so it can wire them into the sender PC.
self.postMessage(
    { command: "sourceTracks", tracks: generators.map(g => g.track) },
    generators.map(g => g.track)
);

let frameIndex = 0;
const frameIntervalUs = 33333;
setInterval(() => {
    const ts = frameIndex * frameIntervalUs;
    // Distinct Y values so the two tracks aren't identical content; U/V neutral
    // gray. The test only checks colorSpace tagging, not pixel values.
    writers[0].write(makeI420Frame(320, 240, 80,  128, 128, ts, senderColorSpaces[0])).catch(() => {});
    writers[1].write(makeI420Frame(320, 240, 180, 128, 128, ts, senderColorSpaces[1])).catch(() => {});
    ++frameIndex;
}, 33);

function sameColorSpace(a, b) {
    return !!a && !!b
        && a.primaries === b.primaries
        && a.transfer === b.transfer
        && a.matrix === b.matrix
        && a.fullRange === b.fullRange;
}

function startReadingTrack(track, trackIndex) {
    const processor = new MediaStreamTrackProcessor({ track });
    const reader = processor.readable.getReader();
    let lastColorSpace = null;
    (async () => {
        while (true) {
            const { value: frame, done } = await reader.read();
            if (done || !frame)
                return;
            const cs = {
                primaries: frame.colorSpace.primaries,
                transfer: frame.colorSpace.transfer,
                matrix: frame.colorSpace.matrix,
                fullRange: frame.colorSpace.fullRange,
            };
            frame.close();
            if (!sameColorSpace(lastColorSpace, cs)) {
                lastColorSpace = cs;
                self.postMessage({ command: "colorSpaceChange", trackIndex, colorSpace: cs });
            }
        }
    })().catch(e => self.postMessage({ command: "error", message: `${e.name}: ${e.message}` }));
}

self.onmessage = (event) => {
    const data = event.data;
    if (data?.command === "process") {
        // Start reader loops for each receiver track. They run forever and post `colorSpaceChange` events on transition.
        data.tracks.forEach((track, i) => startReadingTrack(track, i));
    } else if (data?.command === "swap") {
        // Flip which colorSpace each generator's frames are tagged with. The change ripples through encode → RTP → decode and surfaces back as colorSpaceChange events on the receiver-side reader loops.
        senderColorSpaces.reverse();
    }
};

function unspecifyVP9KeyframeColorSpace(buffer) {
    if (buffer.byteLength < 5)
        return buffer;

    const view = new Uint8Array(buffer);
    const byte0 = view[0];

    // Frame marker is the top two bits — must equal binary 10 for VP9.
    if ((byte0 >> 6) !== 2)
        return buffer;

    const profileLow = (byte0 >> 5) & 1;
    const profileHigh = (byte0 >> 4) & 1;
    const profile = (profileHigh << 1) | profileLow;

    // Bit offset (from MSB of byte 0) to show_existing_frame.
    //   profile 0/1/2: marker(2) + profile(2)             = 4
    //   profile 3:     marker(2) + profile(2) + reserved(1) = 5
    const showExistingBit = profile === 3 ? 5 : 4;

    const peekBit = (offset) => (view[offset >> 3] >> (7 - (offset & 7))) & 1;

    if (peekBit(showExistingBit))
        return buffer; // show_existing_frame=1: no color_config in this frame.

    const frameTypeBit = showExistingBit + 1;
    if (peekBit(frameTypeBit))
        return buffer; // not a keyframe — color_config is absent.

    // After frame_type comes show_frame(1) + error_resilient_mode(1) = 2 bits,
    // then frame_sync_code(24), then for profile >= 2 a ten_or_twelve_bit(1)
    // bit, then color_space(3) — the field we want to zero.
    let colorSpaceBit = showExistingBit + 4 + 24 + (profile >= 2 ? 1 : 0);

    // The 3 bits may straddle a byte boundary depending on profile/alignment.
    // Make a copy so we don't mutate the caller's buffer in place.
    const copy = new Uint8Array(buffer.slice());
    for (let i = 0; i < 3; ++i) {
        const total = colorSpaceBit + i;
        copy[total >> 3] &= ~(1 << (7 - (total & 7)));
    }
    return copy.buffer;
}

self.onrtctransform = event => {
    const transformer = event.transformer;
    const reader = transformer.readable.getReader();
    const writer = transformer.writable.getWriter();
    (async () => {
        while (true) {
            const { value: chunk, done } = await reader.read();
            if (done) {
                await writer.close();
                return;
            }
            chunk.data = unspecifyVP9KeyframeColorSpace(chunk.data);
            await writer.write(chunk);
        }
    })().catch(e => self.postMessage({ command: "error", message: `transform: ${e.name}: ${e.message}` }));
};
