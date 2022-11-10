delete Error.stackTraceLimit

// sourceURL is not materialized
function cacheColumn(o) {
    delete o.sourceURL
}
noInline(cacheColumn)

for (let i = 0; i < 200; ++i) {
    let e = Error()
    cacheColumn(e)
    if (e.sourceURL !== undefined)
        throw "Test failed on iteration " + i + " " + e.sourceURL
    
    if (i == 197) {
        // now it is
        Error.stackTraceLimit = 10
    }
}