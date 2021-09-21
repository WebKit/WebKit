function doesThrow() {
    try {
        1 in ""
        return false
    } catch(v45) {
        return true
    }
}
noInline(doesThrow)
noFTL(doesThrow)

function doesThrowFTL() {
    try {
        1 in ""
        return false
    } catch(v45) {
        return true
    }
}
noInline(doesThrowFTL)

function blackbox() {
    return { }
}
noInline(blackbox)

function doesNotThrow() {
    try {
        1 in blackbox()
        return false
    } catch(v45) {
        return true
    }
}
noInline(doesNotThrow)
noFTL(doesNotThrow)

function trickster(o) {
    try {
        1 in o
        return false
    } catch(v45) {
        return true
    }
}
noInline(trickster)

// Does not throw
function enumeratorTest(o) {
    let sum = 0
    for (let i in o)
        sum += o[i]
    return sum
}
noInline(enumeratorTest)
noInline(enumeratorTest)

let indexedObject = []
indexedObject.length = 10
indexedObject.fill(1)

function main() {
    for (let j = 0; j < 50000; j++) {
        if (!doesThrow())
            throw new Error("Should throw!")
        if (!doesThrowFTL())
            throw new Error("Should throw!")
        if (doesNotThrow())
            throw new Error("Should not throw!")
        
            let o = {}
        o["a" + j] = 0
        if (trickster(o))
            throw new Error("Should not throw!")
        
        enumeratorTest(indexedObject)
    }
    if (!trickster(""))
        throw new Error("Should throw!")
    enumeratorTest("")
}
noDFG(main)
noFTL(main)
noInline(main)
main()