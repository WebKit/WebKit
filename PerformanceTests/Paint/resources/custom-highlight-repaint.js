const N_PARA = 2158; // Long source document
const N_HLPARA = 19; // Every range lives in the first 19 paragraphs.
const N_RANGES = 240; // Registered ranges. Repaint cost is linear in this.
const R_LEN = 4; // Characters per range. The cost should not depend on this.

const LINE = "The quick brown fox jumps over the lazy dog. ".repeat(8);

let paragraphs = [];

function buildDocument() {
    const doc = document.getElementById("doc");
    for (let i = 0; i < N_PARA; ++i) {
        const p = document.createElement("p");
        p.textContent = LINE;
        doc.appendChild(p);
        paragraphs.push(p);
    }
    doc.contentEditable = "true";
    doc.spellcheck = false;
}

// Registering must build FRESH Range objects every time. Re-adding a Highlight
// object that was previously in CSS.highlights leaves it unpainted, and an
// unpainted highlight costs nothing, which would quietly turn this into a
// measurement of the empty case.
function registerHighlights() {
    CSS.highlights.clear();
    const highlight = new Highlight();
    for (let i = 0; i < N_RANGES; ++i) {
        const text = paragraphs[i % N_HLPARA].firstChild;
        const start = 4 + Math.floor(i / N_HLPARA) * 14;
        const range = document.createRange();
        range.setStart(text, start);
        range.setEnd(text, start + R_LEN);
        highlight.add(range);
    }
    CSS.highlights.set("demo", highlight);
}

function cleanUp() {
    document.documentElement.style.removeProperty("margin-right");
    if (window.CSS && CSS.highlights)
        CSS.highlights.clear();
    document.getElementById("doc").textContent = "";
    paragraphs = [];
}

// Measure the cost of one rendering update.
//
// Dirty the whole viewport inside a requestAnimationFrame callback, then measure
// how long it takes for a MessageChannel task posted from that same callback to
// run, which is after style, layout and paint have completed.
let dirtyToggle = 0;
function measureOneRenderingUpdate(onMeasured) {
    const start = PerfTestRunner.now();
    document.documentElement.style.marginRight = (dirtyToggle ^= 1) ? "1px" : "2px";
    const channel = new MessageChannel();
    channel.port1.onmessage = () => {
        onMeasured(PerfTestRunner.now() - start);
    };
    channel.port2.postMessage(0);
}

function runRepaintTest() {
    measureOneRenderingUpdate((elapsed) => {
        if (PerfTestRunner.measureValueAsync(elapsed))
            requestAnimationFrame(runRepaintTest);
    });
}
