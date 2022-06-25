self.addEventListener("message", (event) => {
    performance.clearMarks();
    performance.clearMeasures();
    performance.mark("test-mark");
    performance.measure("test-measure");

    let entries = [...performance.getEntriesByType("mark"), ...performance.getEntriesByType("measure")];
    event.source.postMessage({entries: JSON.stringify(entries)});
});
