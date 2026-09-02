//@ runDefault("--jitPolicyScale=0")

const nesting = 34000;
const callDepth = 8000;
const iterations = testLoopCount;
const stride = 1024;
const emptyCaptures = 22;

function atDepth(depth, callback) {
    const keep = depth;
    if (!depth)
        return callback() + keep;
    return atDepth(depth - 1, callback) + Number(keep === -1);
}

let regexpPayload = "";
for (let i = 0; i < emptyCaptures; ++i)
    regexpPayload += "()";
regexpPayload += "(?=b*(a*))";
regexpPayload += "(?:stablefinalread)?";

const pattern = "(?:".repeat(nesting) + regexpPayload + ")".repeat(nesting);

const pieces = [];
for (let offset = 0; offset < pattern.length; offset += 8000)
    pieces.push(JSON.stringify(pattern.slice(offset, offset + 8000)));
const make = eval(`(function (run) { if (!run) return 1; return new RegExp(${pieces.join(" + ")}, ""); })`);

try { make(true); } catch (e) { }
fullGC();

let exposed = null;
try {
    atDepth(callDepth, () => {
        try { new RegExp(pattern, ""); } catch (e) { }
        for (let i = 0; i < iterations && !exposed; ++i) {
            make(false);
            if ((i % stride) === stride - 1)
                try { exposed = make(true); } catch (e) { }
        }
        if (!exposed)
            try { exposed = make(true); } catch (e) { }
        return 0;
    });
} catch (e) { }

if (exposed) {
    const subject = "a".repeat(113);
    for (let i = 0; i < 6; ++i) {
        try { exposed.test(subject); } catch (e) { }
    }
}
