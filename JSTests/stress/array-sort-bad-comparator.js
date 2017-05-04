//@ runDefault

function test() {
    try {
        [1,2].sort(true);
        return false;
    } catch (etrue) {}
    try {
        [1,2].sort({});
        return false;
    } catch (eobj) {}
    try {
        [1,2].sort([]);
        return false;
    } catch (earr) {}
    try {
        [1,2].sort(/a/g);
        return false;
    } catch (eregex) {}
    return true;
}

if(!test())
    throw new Error("Bad result");
