const L = 100;
const M = 200;
const S = 0;
const E = 256;
const T = (E - S) - 5;

for (let i = 0; i < 2; i++) {
  let g = (a) => a.size;
  let d = [];

  for (let c = 0; c < 16; c++) {
    g(d);

    try {
        const g = (a) => a.byteLength;
        const b = new ArrayBuffer(M, { maxByteLength: M });
        const d = new DataView(b, 0, L);

        for (let c = S; c < E + 11; c++) {
          g(1);

          if (c != E) {
            for (let c = S; c < E; c++) {
              try { if (c == E - T) { g(1); } else { g(d); } } catch(e) { }
            }

            if (c != E) {
              for (let c = S; c < E; c++) {
                try { if (c == E - T) { g(1); } else { g(d); } } catch(e) { }
              }
              if (c != E) {
                for (let c = S; c < E; c++) {
                  try { if (c == E - T) { g(1); } else { g(d); } } catch(e) { }
                }
              }
            }
          }

          if (c == E) { b.resize(L - 5); } else { g(d); }
      }
    } catch (e) { }
  }
}
