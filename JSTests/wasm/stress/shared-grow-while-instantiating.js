//@ requireOptions("--useWasmIPInt=0")
//@ $skipModes << "wasm-no-wasm-jit".to_sym
//@ $skipModes << "wasm-no-jit".to_sym

const bytes = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 0, 1, 127, 2, 17, 1, 3, 101, 110, 118, 6, 109, 101, 109, 111, 114, 121, 2, 3, 1, 128, 8, 3, 2, 1, 0, 7, 8, 1, 4, 115, 105, 122, 101, 0, 0, 10, 6, 1, 4, 0, 63, 0, 11]);
const maximum = 256;
const memory = new WebAssembly.Memory({ initial: 1, maximum, shared: true });
const module = new WebAssembly.Module(bytes);
const kept = new WebAssembly.Instance(module, { env: { memory } });
const byteList = [...bytes].join(",");

const sweeper = `
const bytes = new Uint8Array([${byteList}]);
const module = new WebAssembly.Module(bytes);
function fill(memory) {
    for (let i = 0; i < 40; ++i)
        new WebAssembly.Instance(module, { env: { memory } });
}
let incoming = null;
$.agent.receiveBroadcast(function (memory) { incoming = memory; });
while (incoming === null)
    $.agent.sleep(1);
fill(incoming);
incoming = null;
gc();
$.agent.report("swept");
$.agent.leaving();
`;

const grower = `
$.agent.receiveBroadcast(function (memory) {
    let grown = 0;
    while (true) {
        try {
            memory.grow(1);
            ++grown;
        } catch (error) {
            $.agent.report("grow-done " + grown);
            $.agent.leaving();
            return;
        }
    }
});
`;

const sweepers = 4;
for (let i = 0; i < sweepers; ++i)
    $.agent.start(sweeper);
$.agent.start(grower);
$.agent.broadcast(memory);

let reports = 0;
while (reports < sweepers + 1) {
    const report = $.agent.getReport();
    if (report === null) {
        $.agent.sleep(10);
        continue;
    }
    if (typeof report !== "string" || (!report.startsWith("swept") && !report.startsWith("grow-done")))
        throw new Error(String(report));
    ++reports;
}

const pages = memory.buffer.byteLength / 65536;
const cached = kept.exports.size();
if (cached !== pages)
    throw new Error("cached size " + cached + " buffer pages " + pages);
if (pages !== maximum)
    throw new Error("grew to " + pages);
