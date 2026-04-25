//@ runDefault("--jitPolicyScale=0")
let obj = {
    p: [{
        q: [{
            x: 0,
            w: [{
                y: [{
                    a: [ {
                        b: [{
                            c: [{
                                d: [{}, {}, {}]
                            }]
                        }]
                    }]
                }, {}, {}]
            }]
        }]
    }]
};

function dumpObj(obj) {
    let output = '' ** ''
    mallocInALoop();
    mallocInALoop();
    for (let item in obj) {
        let child = obj[item];
        output += dumpObj(child);
    }
    return output;
}
noInline(dumpObj);

dumpObj(obj);
