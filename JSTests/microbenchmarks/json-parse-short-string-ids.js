//@ $skipModes << :lockdown if $buildType == "debug"

let seed = 12345;
function next() {
    seed = (seed + 0x9e3779b9) | 0;
    let z = seed;
    z = Math.imul(z ^ (z >>> 16), 0x85ebca6b);
    z = Math.imul(z ^ (z >>> 13), 0xc2b2ae35);
    return (z ^ (z >>> 16)) >>> 0;
}

const colors = ["red", "blue", "green", "black", "white"];
const sizes = ["S", "M", "L", "XL"];
const items = [];
for (let i = 0; i < 2000; ++i) {
    items.push({
        id: "item-" + (1000 + next() % 9000) + "-" + colors[next() % colors.length],
        sku: "SKU-" + (10000 + next() % 90000) + "-" + sizes[next() % sizes.length],
        owner: "user-" + (next() % 100000).toString(36),
        parent: "ord-" + (next() % 1e6),
        status: next() & 1 ? "active" : "archived",
    });
}
const json = JSON.stringify({ items });

// Each payload carries a distinct set of short ID-like string values, as successive API responses would.
const separators = "GHIJKLMNOPQRSTUVWXYZghijklmnopqrstuvwxyz";
const payloads = [];
for (let i = 0; i < separators.length; ++i)
    payloads.push(json.replaceAll("-", separators[i]));

for (let i = 0; i < 240; ++i)
    JSON.parse(payloads[i % payloads.length]);
