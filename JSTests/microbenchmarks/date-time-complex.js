// Toggle `complex` by hand to measure alternating-locale constructions.
const complex = false;
const iterations = 1000000;
let dtf;

if (!complex) {
    for (let i = 0; i < iterations; i++) {
        dtf = new Intl.DateTimeFormat();
    }
} else {
    for (let i = 0; i < iterations; i++) {
        dtf = new Intl.DateTimeFormat(i % 2 ? "en-US" : "en-GB");
    }
}
