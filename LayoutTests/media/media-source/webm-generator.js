// webm-generator.js — In-memory WebM generator for MSE layout tests.
//
// Generates valid WebM init segments and media segments (clusters) that go
// through the real SourceBufferParserWebM pipeline. Mirrors the API of
// mp4-generator.js so tests can be written against either container format.
//
// Usage:
//   const { init, media } = WebM.samples({
//       timescale: 1000,
//       track: { id: 1, type: 'video' },
//       samples: [
//           { pts: 0, dts: 0, duration: 1000, isSync: true },
//           { pts: 1000, dts: 1000, duration: 1000, isSync: false },
//       ]
//   });
//   sourceBuffer.appendBuffer(init);
//   // ... wait for updateend ...
//   sourceBuffer.appendBuffer(media);

const WebM = (function() {

    // ========================================================================
    // Binary Utilities
    // ========================================================================

    function concat(...arrays) {
        let totalLength = 0;
        for (const arr of arrays)
            totalLength += arr.byteLength;
        const result = new Uint8Array(totalLength);
        let offset = 0;
        for (const arr of arrays) {
            result.set(arr instanceof Uint8Array ? arr : new Uint8Array(arr), offset);
            offset += arr.byteLength;
        }
        return result;
    }

    // ========================================================================
    // EBML Variable-Length Integer Encoding
    // ========================================================================

    // Encode a value as an EBML VINT (variable-length integer) size field.
    // The leading bits indicate the width: 1xxxxxxx = 1 byte, 01xxxxxx = 2, etc.
    function vintSize(value) {
        if (value < 0x7F)
            return new Uint8Array([0x80 | value]);
        if (value < 0x3FFF)
            return new Uint8Array([0x40 | (value >> 8), value & 0xFF]);
        if (value < 0x1FFFFF)
            return new Uint8Array([0x20 | (value >> 16), (value >> 8) & 0xFF, value & 0xFF]);
        if (value < 0x0FFFFFFF)
            return new Uint8Array([0x10 | (value >> 24), (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF]);
        // For larger values, use 8-byte encoding
        const hi = Math.floor(value / 0x100000000);
        const lo = value >>> 0;
        return new Uint8Array([
            0x01, (hi >> 24) & 0xFF, (hi >> 16) & 0xFF, (hi >> 8) & 0xFF,
            hi & 0xFF, (lo >> 24) & 0xFF, (lo >> 16) & 0xFF, lo & 0xFF
        ]);
    }

    // EBML "unknown size" marker — used for Segment and Cluster elements in
    // streaming/MSE mode so the parser doesn't need the total size upfront.
    const UNKNOWN_SIZE = new Uint8Array([0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]);

    // Encode an element ID as raw bytes. Element IDs are NOT VINTs — they use
    // the same variable-width encoding but the leading bits are part of the ID.
    function idBytes(id) {
        if (id <= 0xFF)
            return new Uint8Array([id]);
        if (id <= 0xFFFF)
            return new Uint8Array([id >> 8, id & 0xFF]);
        if (id <= 0xFFFFFF)
            return new Uint8Array([(id >> 16) & 0xFF, (id >> 8) & 0xFF, id & 0xFF]);
        return new Uint8Array([(id >> 24) & 0xFF, (id >> 16) & 0xFF, (id >> 8) & 0xFF, id & 0xFF]);
    }

    // ========================================================================
    // EBML Element Builders
    // ========================================================================

    function ebmlElement(id, payload) {
        return concat(idBytes(id), vintSize(payload.byteLength), payload);
    }

    function ebmlMaster(id, ...children) {
        return ebmlElement(id, concat(...children));
    }

    // Master element with unknown size (for Segment, Cluster)
    function ebmlMasterUnknownSize(id, ...children) {
        return concat(idBytes(id), UNKNOWN_SIZE, ...children);
    }

    function ebmlUint(id, value) {
        // Encode as minimum-width big-endian unsigned integer
        if (value === 0)
            return ebmlElement(id, new Uint8Array([0]));
        const bytes = [];
        let v = value;
        while (v > 0) {
            bytes.unshift(v & 0xFF);
            v = Math.floor(v / 256);
        }
        return ebmlElement(id, new Uint8Array(bytes));
    }

    function ebmlString(id, str) {
        const bytes = new Uint8Array(str.length);
        for (let i = 0; i < str.length; i++)
            bytes[i] = str.charCodeAt(i);
        return ebmlElement(id, bytes);
    }

    function ebmlFloat8(id, value) {
        const buf = new ArrayBuffer(8);
        new DataView(buf).setFloat64(0, value, false);
        return ebmlElement(id, new Uint8Array(buf));
    }

    function ebmlBinary(id, data) {
        return ebmlElement(id, data);
    }

    // ========================================================================
    // Element IDs
    // ========================================================================

    const ID = {
        EBML:               0x1A45DFA3,
        EBMLVersion:        0x4286,
        EBMLReadVersion:    0x42F7,
        EBMLMaxIDLength:    0x42F2,
        EBMLMaxSizeLength:  0x42F3,
        DocType:            0x4282,
        DocTypeVersion:     0x4287,
        DocTypeReadVersion: 0x4285,

        Segment:            0x18538067,
        Info:               0x1549A966,
        TimecodeScale:      0x2AD7B1,
        Duration:           0x4489,
        MuxingApp:          0x4D80,
        WritingApp:         0x5741,

        Tracks:             0x1654AE6B,
        TrackEntry:         0xAE,
        TrackNumber:        0xD7,
        TrackUID:           0x73C5,
        TrackType:          0x83,
        FlagLacing:         0x9C,
        DefaultDuration:    0x23E383,
        CodecID:            0x86,
        CodecPrivate:       0x63A2,
        CodecDelay:         0x56AA,
        SeekPreRoll:        0x56BB,

        Video:              0xE0,
        PixelWidth:         0xB0,
        PixelHeight:        0xBA,

        Audio:              0xE1,
        SamplingFrequency:  0xB5,
        Channels:           0x9F,

        Cluster:            0x1F43B675,
        Timecode:           0xE7,
        SimpleBlock:        0xA3,
        BlockGroup:         0xA0,
        Block:              0xA1,
        BlockDuration:      0x9B,
        ReferenceBlock:     0xFB,
    };

    // ========================================================================
    // Codec Configuration
    // ========================================================================

    // Generated with: ffmpeg -f lavfi -i "color=c=black:s=320x240:r=24" -frames:v 2
    //   -c:v libvpx -b:v 100k -auto-alt-ref 0 -f webm
    // VP8, 320x240

    const VIDEO_WIDTH = 320;
    const VIDEO_HEIGHT = 240;

    const VIDEO_CODEC_STRING = 'vp8';
    const AUDIO_CODEC_STRING = 'opus';

    // Valid VP8 keyframe for a black 320x240 frame (160 bytes)
    const VP8_KEYFRAME = new Uint8Array([
        0x30, 0x12, 0x00, 0x9D, 0x01, 0x2A, 0x40, 0x01, 0xF0, 0x00, 0x00, 0x47,
        0x08, 0x85, 0x85, 0x88, 0x85, 0x84, 0x88, 0x02, 0x02, 0x00, 0x06, 0x16,
        0x04, 0xF7, 0x06, 0x81, 0x64, 0x9F, 0x6B, 0xDB, 0x9B, 0x27, 0x38, 0x7B,
        0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B,
        0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B,
        0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B,
        0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B,
        0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B,
        0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B,
        0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B,
        0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B,
        0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B,
        0x27, 0x38, 0x7B, 0x27, 0x38, 0x7B, 0x27, 0x38, 0x7A, 0xF4, 0x00, 0xFE,
        0xFF, 0xAB, 0x50, 0x80
    ]);

    // Valid VP8 interframe (P-frame) for a black 320x240 frame (26 bytes)
    const VP8_INTERFRAME = new Uint8Array([
        0xD1, 0x02, 0x00, 0x01, 0x10, 0x10, 0x00, 0x18, 0x00, 0x18, 0x58, 0x2F,
        0xF4, 0x00, 0x08, 0x80, 0x04, 0x33, 0x5F, 0xAD, 0x72, 0x4F, 0x9C, 0x73,
        0x00, 0x00
    ]);

    // OpusHead codec private data: version 1, stereo, 48000 Hz, no pre-skip
    // Generated with: ffmpeg -f lavfi -i "anullsrc=r=48000:cl=stereo" -c:a libopus -f webm
    const OPUS_HEAD = new Uint8Array([
        0x4F, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64, // "OpusHead"
        0x01,       // version
        0x02,       // channels
        0x00, 0x00, // pre-skip (0, little-endian)
        0x80, 0xBB, 0x00, 0x00, // sample rate (48000, little-endian)
        0x00, 0x00, // output gain
        0x00        // channel mapping family
    ]);

    // Valid Opus silence frame (3 bytes). TOC byte 0xFC = config 31 (20ms frame).
    // IMPORTANT: Audio sample durations in tests must be 20 (ms) to match the
    // actual codec frame duration. Mismatched container/codec durations produce
    // gaps rather than contiguous audio.
    const OPUS_SILENCE = new Uint8Array([0xFC, 0xFF, 0xFE]);

    // ========================================================================
    // Init Segment Builders
    // ========================================================================

    function ebmlHeader() {
        return ebmlMaster(ID.EBML,
            ebmlUint(ID.EBMLVersion, 1),
            ebmlUint(ID.EBMLReadVersion, 1),
            ebmlUint(ID.EBMLMaxIDLength, 4),
            ebmlUint(ID.EBMLMaxSizeLength, 8),
            ebmlString(ID.DocType, 'webm'),
            ebmlUint(ID.DocTypeVersion, 2),
            ebmlUint(ID.DocTypeReadVersion, 2)
        );
    }

    function infoElement() {
        return ebmlMaster(ID.Info,
            ebmlUint(ID.TimecodeScale, 1000000), // 1ms timecode units
            ebmlString(ID.MuxingApp, 'WebKit'),
            ebmlString(ID.WritingApp, 'WebKit')
        );
    }

    function trackEntry(trackDef) {
        const isVideo = trackDef.type === 'video';
        const children = [
            ebmlUint(ID.TrackNumber, trackDef.id),
            ebmlUint(ID.TrackUID, trackDef.id),
            ebmlUint(ID.TrackType, isVideo ? 1 : 2),
            ebmlUint(ID.FlagLacing, 0),
            ebmlString(ID.CodecID, isVideo ? 'V_VP8' : 'A_OPUS'),
        ];

        // DefaultDuration in nanoseconds. Required for parsers (e.g. GStreamer)
        // that don't infer frame duration from timecode gaps.
        // Defaults to 1000ms for video, 20ms for Opus audio.
        // Pass defaultDuration: 0 to omit the element entirely.
        const defaultDur = trackDef.defaultDuration !== undefined
            ? trackDef.defaultDuration
            : (isVideo ? 1000 : 20);
        if (defaultDur > 0)
            children.push(ebmlUint(ID.DefaultDuration, defaultDur * 1000000));

        if (isVideo) {
            children.push(ebmlMaster(ID.Video,
                ebmlUint(ID.PixelWidth, trackDef.width || VIDEO_WIDTH),
                ebmlUint(ID.PixelHeight, trackDef.height || VIDEO_HEIGHT)
            ));
        } else {
            children.push(
                ebmlMaster(ID.Audio,
                    ebmlFloat8(ID.SamplingFrequency, trackDef.sampleRate || 48000),
                    ebmlUint(ID.Channels, trackDef.channels || 2)
                ),
                ebmlUint(ID.CodecDelay, 0),
                ebmlUint(ID.SeekPreRoll, 80000000),
                ebmlBinary(ID.CodecPrivate, OPUS_HEAD)
            );
        }

        return ebmlMaster(ID.TrackEntry, ...children);
    }

    function tracksElement(tracks) {
        return ebmlMaster(ID.Tracks, ...tracks.map(t => trackEntry(t)));
    }

    // ========================================================================
    // Media Segment Builders (Cluster + BlockGroup)
    // ========================================================================

    function blockGroup(trackNumber, relativeTimecode, isKeyframe, duration, frameData) {
        // Block payload: trackNumber(VINT) + timecode(int16 BE) + flags(byte) + frame
        const trackVint = new Uint8Array([0x80 | trackNumber]);
        const tc = new Uint8Array(2);
        new DataView(tc.buffer).setInt16(0, relativeTimecode, false);
        const flags = new Uint8Array([0x00]); // Block flags (no lacing)
        const blockPayload = concat(trackVint, tc, flags, frameData);

        const children = [
            ebmlElement(ID.Block, blockPayload),
            ebmlUint(ID.BlockDuration, duration),
        ];

        // Non-keyframes must have a ReferenceBlock pointing to a previous frame.
        // Value 0 means "references a frame at this block's timecode - 0" which
        // is the conventional way to signal a non-keyframe.
        if (!isKeyframe)
            children.push(ebmlUint(ID.ReferenceBlock, 0));

        return ebmlMaster(ID.BlockGroup, ...children);
    }

    // ========================================================================
    // Public API
    // ========================================================================

    return {
        VIDEO_CODEC: VIDEO_CODEC_STRING,
        AUDIO_CODEC: AUDIO_CODEC_STRING,

        VIDEO_TYPE: `video/webm; codecs="${VIDEO_CODEC_STRING}"`,
        AUDIO_TYPE: `audio/webm; codecs="${AUDIO_CODEC_STRING}"`,

        concat,

        // Generate a valid WebM initialization segment (EBML header + Segment(Info + Tracks)).
        //
        // options.tracks[] - Array of track definitions:
        //   .id       - Track number (integer, 1-based)
        //   .type     - 'video' or 'audio'
        //   .width    - Video width (optional, default 320)
        //   .height   - Video height (optional, default 240)
        initSegment(options) {
            const tracks = options.tracks || [{ id: 1, type: 'video' }];
            return concat(
                ebmlHeader(),
                ebmlMasterUnknownSize(ID.Segment,
                    infoElement(),
                    tracksElement(tracks)
                )
            );
        },

        // Generate a valid WebM media segment (Cluster with BlockGroups).
        // Each frame gets an explicit BlockDuration so parsers don't need
        // to infer duration from timecode gaps.
        //
        // options.tracks[] - Array of track data:
        //   .id              - Track number (must match init segment)
        //   .type            - 'video' or 'audio'
        //   .baseDecodeTime  - Cluster timecode in milliseconds
        //   .samples[]       - Array of sample descriptors:
        //     .duration    - Sample duration in milliseconds
        //     .isSync      - true for keyframe
        //     .data        - Override frame data (Uint8Array, optional)
        mediaSegment(options) {
            const tracks = options.tracks;
            const clusterTimecode = (tracks[0] && tracks[0].baseDecodeTime) || 0;

            // Block relative timecodes are int16 (max 32767). If cumulative
            // durations exceed this, we must split into multiple clusters.
            const MAX_RELATIVE_TC = 32767;
            const clusters = [];
            let currentBlocks = [];
            let currentClusterBase = clusterTimecode;
            let relativeTime = 0;

            for (const track of tracks) {
                const trackType = track.type || 'video';
                relativeTime = 0;

                for (const sample of track.samples) {
                    if (relativeTime > MAX_RELATIVE_TC && currentBlocks.length > 0) {
                        clusters.push(ebmlMasterUnknownSize(ID.Cluster,
                            ebmlUint(ID.Timecode, currentClusterBase),
                            ...currentBlocks.map(b =>
                                blockGroup(b.trackId, b.relativeTime, b.isSync, b.duration, b.data))
                        ));
                        currentClusterBase += relativeTime;
                        relativeTime = 0;
                        currentBlocks = [];
                    }

                    const frameData = sample.data
                        || (trackType === 'video'
                            ? (sample.isSync ? VP8_KEYFRAME : VP8_INTERFRAME)
                            : OPUS_SILENCE);
                    currentBlocks.push({
                        trackId: track.id,
                        relativeTime,
                        isSync: sample.isSync,
                        duration: sample.duration,
                        data: frameData,
                    });
                    relativeTime += sample.duration;
                }
            }

            // Flush remaining blocks
            if (currentBlocks.length > 0) {
                clusters.push(ebmlMasterUnknownSize(ID.Cluster,
                    ebmlUint(ID.Timecode, currentClusterBase),
                    ...currentBlocks.map(b =>
                        blockGroup(b.trackId, b.relativeTime, b.isSync, b.duration, b.data))
                ));
            }

            return concat(...clusters);
        },

        // Convenience: generate init + media from a flat sample list.
        // API matches mp4-generator.js for easy test porting.
        //
        // Returns { init: Uint8Array, media: Uint8Array, mimeType: string }
        samples(options) {
            const timescale = options.timescale || 1000;

            if (options.track) {
                const track = { id: options.track.id || 1, type: options.track.type || 'video' };
                const rawSamples = options.samples || [];
                const scaleFactor = 1000 / timescale; // convert to ms
                const baseDecodeTime = rawSamples.length
                    ? Math.max(0, Math.min(...rawSamples.map(s => (s.dts !== undefined ? s.dts : s.pts)))) * scaleFactor
                    : 0;
                const samples = rawSamples.map(s => ({
                    duration: s.duration * scaleFactor,
                    isSync: s.isSync !== undefined ? s.isSync : !!(s.flags & 1),
                    data: s.data,
                }));

                const init = this.initSegment({ tracks: [track] });
                const media = this.mediaSegment({
                    tracks: [{
                        id: track.id, type: track.type,
                        baseDecodeTime, samples,
                    }]
                });
                const mimeType = (track.type === 'video') ? this.VIDEO_TYPE : this.AUDIO_TYPE;
                return { init, media, mimeType };
            }

            // Multi-track mode
            const tracks = (options.tracks || []).map(t => ({
                id: t.id, type: t.type || 'video',
            }));
            const init = this.initSegment({ tracks });
            const trackSamples = options.trackSamples || {};
            const scaleFactor = 1000 / (options.timescale || 1000);
            const media = this.mediaSegment({
                tracks: tracks.map(t => {
                    const raw = trackSamples[t.id] || [];
                    const baseDecodeTime = raw.length
                        ? Math.max(0, Math.min(...raw.map(s => (s.dts !== undefined ? s.dts : s.pts)))) * scaleFactor
                        : 0;
                    return {
                        id: t.id, type: t.type, baseDecodeTime,
                        samples: raw.map(s => ({
                            duration: s.duration * scaleFactor,
                            isSync: s.isSync !== undefined ? s.isSync : !!(s.flags & 1),
                        })),
                    };
                }),
            });

            const hasVideo = tracks.some(t => t.type === 'video');
            return { init, media, mimeType: hasVideo ? this.VIDEO_TYPE : this.AUDIO_TYPE };
        },

        // Generate only a media segment from a flat sample list.
        mediaSamples(options) {
            const timescale = options.timescale || 1000;
            const track = options.track || { id: 1, type: 'video' };
            const rawSamples = options.samples || [];
            const scaleFactor = 1000 / timescale;
            const baseDecodeTime = rawSamples.length
                ? Math.max(0, Math.min(...rawSamples.map(s => (s.dts !== undefined ? s.dts : s.pts)))) * scaleFactor
                : 0;
            return this.mediaSegment({
                tracks: [{
                    id: track.id || 1, type: track.type || 'video',
                    baseDecodeTime,
                    samples: rawSamples.map(s => ({
                        duration: s.duration * scaleFactor,
                        isSync: s.isSync !== undefined ? s.isSync : !!(s.flags & 1),
                        data: s.data,
                    })),
                }]
            });
        },
    };
})();
