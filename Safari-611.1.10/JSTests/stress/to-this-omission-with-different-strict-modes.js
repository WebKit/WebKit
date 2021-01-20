function thisA() {
    return this.a
}
function thisAStrictWrapper() {
    'use strict';
    thisA.apply(this);
}
let x = false;
for (let j=0; j<1e4; j++)
    thisAStrictWrapper.call(x);
