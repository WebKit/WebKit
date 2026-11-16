// caption-generator.js — CEA-608 closed-caption encoder for layout tests.
//
// Exposes a global `Captions` class with a nested `CEA608` class and two
// methods for building caption samples for use with MP4.samples():
//
//   Captions.buildPaintOnCueSamples({ cues, totalDurationTicks })
//     Paint-on style — text must be exactly 2 chars per cue.
//
//   Captions.buildPopOnCueSamples({ cues, totalDurationTicks })
//     Pop-on style — text may be any length.
//
// Both return [{data: Uint8Array, duration: number}] suitable for passing as
// the captions track in MP4.samples() trackSamples.

// ============================================================================
// Module-private helpers
// ============================================================================

// CEA-608 odd parity: bit 7 is set so the whole byte has an odd population count.
function oddParity(byte) {
    let bits = byte & 0x7F;
    bits ^= bits >> 4;
    bits ^= bits >> 2;
    bits ^= bits >> 1;
    return (bits & 1) ? (byte & 0x7F) : ((byte & 0x7F) | 0x80);
}

function pairsToBytes(pairs) {
    const out = new Uint8Array(pairs.length * 2);
    for (let i = 0; i < pairs.length; i++) {
        out[i * 2]     = oddParity(pairs[i][0]);
        out[i * 2 + 1] = oddParity(pairs[i][1]);
    }
    return out;
}

function cdatBox(bytes) {
    const CDAT = Uint8Array.from('cdat', char => char.charCodeAt(0));
    const out = new Uint8Array(8 + bytes.byteLength);
    new DataView(out.buffer).setUint32(0, out.byteLength, false);
    out.set(CDAT, 4);
    out.set(bytes, 8);
    return out;
}

// ============================================================================
// CEA-608 — Channel 1 / Field 1
// ============================================================================

// Row → [byte1, isPair2]. Row 11 is pair-1 only (byte1 = 0x10 is special).
const ROW_TABLE = [
    [0x11, false], [0x11, true],
    [0x12, false], [0x12, true],
    [0x15, false], [0x15, true],
    [0x16, false], [0x16, true],
    [0x17, false], [0x17, true],
    [0x10, false],
    [0x13, false], [0x13, true],
    [0x14, false], [0x14, true],
];

const COLOR_NIBBLE = {
    white:   0x0,
    green:   0x2,
    blue:    0x4,
    cyan:    0x6,
    red:     0x8,
    yellow:  0xA,
    magenta: 0xC,
};

class CEA608 {
    // Control-code byte pairs (no parity yet — pairsToBytes applies it).
    static RCL = [0x14, 0x20];  // Resume Caption Loading (pop-on)
    static RDC = [0x14, 0x29];  // Resume Direct Captioning (paint-on)
    static ENM = [0x14, 0x2E];  // Erase Non-displayed Memory
    static EDM = [0x14, 0x2C];  // Erase Displayed Memory
    static EOC = [0x14, 0x2F];  // End Of Caption (swap + display)
    static CR  = [0x14, 0x2D];  // Carriage Return

    // Preamble Address Code. See CEA-608-E Table 3.
    //   row    : 1..15
    //   indent : 0, 4, 8, 12, 16, 20, 24, 28 (columns; 0 means color mode)
    //   color  : one of COLOR_NIBBLE keys (used only when indent === 0)
    //   italic : boolean (used only when indent === 0; overrides color)
    //   underline : boolean
    static pac(row, indent = 0, { color = 'white', underline = false, italic = false } = {}) {
        const [byte1, pair2] = ROW_TABLE[row - 1];
        const pair2Bit = pair2 ? 0x20 : 0;
        if (indent > 0) {
            const nibble = Math.min(14, Math.floor(indent / 4) * 2);
            return [byte1, 0x50 | pair2Bit | nibble | (underline ? 0x01 : 0)];
        }
        const colorNibble = italic ? 0xE : (COLOR_NIBBLE[color] ?? 0x0);
        return [byte1, 0x40 | pair2Bit | colorNibble | (underline ? 0x01 : 0)];
    }

    // Map ASCII to 7-bit byte pairs. Non-ASCII falls back to '?'. Odd-length
    // strings pad the last pair's right byte with 0x00 (null — ignored by the decoder).
    static textPairs(text) {
        const safe = charCode => (charCode >= 0x20 && charCode <= 0x7F) ? charCode : 0x3F;
        const pairs = [];
        for (let i = 0; i < text.length; i += 2) {
            const first  = safe(text.charCodeAt(i));
            const second = (i + 1 < text.length) ? safe(text.charCodeAt(i + 1)) : 0x00;
            pairs.push([first, second]);
        }
        return pairs;
    }

    // RCL + ENM + PAC + text + EOC, serialized with odd parity applied.
    static popOn(text, { row = 15, indent = 0, ...style } = {}) {
        return pairsToBytes([
            CEA608.RCL,
            CEA608.ENM,
            CEA608.pac(row, indent, style),
            ...CEA608.textPairs(text),
            CEA608.EOC,
        ]);
    }

    // EDM + RDC + PAC + 2-char text pair, serialized with odd parity applied.
    // The leading EDM clears the display (closing the previous cue), then RDC
    // sets paint-on mode and the single text pair writes directly to the display.
    // Text must be exactly 2 chars — one pair produces exactly one delivery.
    static paintOn(text, { row = 15, ...style } = {}) {
        if (text.length !== 2)
            throw new Error(`CEA608.paintOn: text must be exactly 2 chars, got "${text}"`);
        return pairsToBytes([
            CEA608.EDM,
            CEA608.RDC,
            CEA608.pac(row, 0, style),
            ...CEA608.textPairs(text),
        ]);
    }

    // EDM — clears displayed memory.
    static clear() {
        return pairsToBytes([CEA608.EDM]);
    }

    // Escape hatch: one raw pair with parity applied.
    static raw(byte1, byte2) {
        return pairsToBytes([[byte1, byte2]]);
    }
}

// ============================================================================
// Public API
// ============================================================================

class Captions {
    static CEA608 = CEA608;

    // Build caption samples using paint-on mode — one 2-char text pair per sample.
    //
    // RDC paint-on mode writes each 2-byte text pair directly to the display,
    // producing exactly one attributed-string delivery per sample. Text must be
    // exactly 2 chars. A final EDM sample is appended to close the last cue.
    //
    //   cues               — [{text: '2-char string', durationTicks}]
    //   totalDurationTicks — total caption track duration
    //
    // Returns [{data: Uint8Array, duration: number}].
    static buildPaintOnCueSamples({ cues, totalDurationTicks }) {
        const samples = [];
        let usedTicks = 0;
        for (const { text, durationTicks } of cues) {
            samples.push({ data: cdatBox(CEA608.paintOn(text)), duration: durationTicks });
            usedTicks += durationTicks;
        }

        // Final EDM closes the last cue with a count=0 delivery.
        samples.push({ data: cdatBox(CEA608.clear()), duration: totalDurationTicks - usedTicks });
        return samples;
    }

    // Build caption samples using pop-on mode — one complete pop-on sequence per sample.
    //
    // Each sample contains RCL+ENM+PAC+text+EOC, which buffers the text invisibly
    // and flips it to the display on EOC, producing exactly one attributed-string
    // delivery regardless of text length. A final EDM sample closes the last cue.
    //
    //   cues               — [{text, durationTicks}]  (text may be any length)
    //   totalDurationTicks — total caption track duration
    //
    // Returns [{data: Uint8Array, duration: number}].
    static buildPopOnCueSamples({ cues, totalDurationTicks }) {
        const samples = [];
        let usedTicks = 0;
        for (const { text, durationTicks } of cues) {
            samples.push({ data: cdatBox(CEA608.popOn(text)), duration: durationTicks });
            usedTicks += durationTicks;
        }

        // Final EDM closes the last cue with a count=0 delivery.
        samples.push({ data: cdatBox(CEA608.clear()), duration: totalDurationTicks - usedTicks });
        return samples;
    }
}
