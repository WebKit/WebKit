//@ requireOptions("--useRandomizingFuzzerAgent=1", "--jitPolicyScale=0", "--useConcurrentJIT=0")
const f = Uint8Array.of;
for (let i=0; i<2; i++) {
  try {
    f();
  } catch {}
}
