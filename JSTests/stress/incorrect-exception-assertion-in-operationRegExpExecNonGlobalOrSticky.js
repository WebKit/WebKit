//@ runDefault("--alwaysUseShadowChicken=true", "--jitPolicyScale=0", "--useRandomizingFuzzerAgent=1", "--maxPerThreadStackUsage=1572863")
//@ slow!

class C {
    constructor(func) {
        this.func = func;
    }
    runTest() {
        this.func();
    }
}
function recurseAndTest() {
    try {
        recurseAndTest();
        test.runTest();
    } catch (e) {
    }
}
const howManyParentheses = 1000;
const deepRE = new RegExp('('.repeat(howManyParentheses) + ')'.repeat(howManyParentheses));
let test = 
    new C(() => {
        deepRE.exec('');
    });

recurseAndTest();
