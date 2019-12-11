//@ runNoFTL

function foo () {
try{}catch(e){}print(e);let e;
}

try {
    foo();
} catch (e) {}

