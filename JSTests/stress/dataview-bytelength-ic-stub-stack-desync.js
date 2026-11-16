let ab = new ArrayBuffer(64, { maxByteLength: 1024 });
let dv = new DataView(ab);
let decoy = { byteLength: 7 };
let decoy2 = { byteLength: 7, x: 1 };
let decoy3 = { byteLength: 7, y: 1 };

let objs = [];
for (let i = 0; i < 64; i++) objs.push({marker: 0x1337 + i});

function hot(o, a, b) {
    let p0=b[0], p1=b[1], p2=b[2], p3=b[3], p4=b[4], p5=b[5], p6=b[6], p7=b[7],
        p8=b[8], p9=b[9], p10=b[10], p11=b[11], p12=b[12], p13=b[13], p14=b[14], p15=b[15],
        p16=b[16], p17=b[17], p18=b[18], p19=b[19], p20=b[20], p21=b[21], p22=b[22], p23=b[23],
        p24=b[24], p25=b[25], p26=b[26], p27=b[27], p28=b[28], p29=b[29], p30=b[30], p31=b[31];
    let len;
    try {
        len = o.byteLength;
    } catch (e) {
        len = -1;
    }
    return [len, p0.marker, p1.marker, p2.marker, p3.marker, p4.marker, p5.marker, p6.marker, p7.marker,
            p8.marker, p9.marker, p10.marker, p11.marker, p12.marker, p13.marker, p14.marker, p15.marker,
            p16.marker, p17.marker, p18.marker, p19.marker, p20.marker, p21.marker, p22.marker, p23.marker,
            p24.marker, p25.marker, p26.marker, p27.marker, p28.marker, p29.marker, p30.marker, p31.marker];
}
noInline(hot);

let A = new Int32Array(64);
for (let i = 0; i < 64; i++) A[i] = i + 1;

for (let i = 0; i < 200000; i++) {
    hot(decoy, A, objs);
    hot(decoy2, A, objs);
    hot(decoy3, A, objs);
    hot(dv, A, objs);
}

for (let i = 0; i < 100; i++) {
    hot(dv, A, objs);
}

ab.transfer();

let r = hot(dv, A, objs);
