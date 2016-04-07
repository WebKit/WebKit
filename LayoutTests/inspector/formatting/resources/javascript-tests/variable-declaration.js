var a;
var a=1;
var a,b;
var a=1,b;
var a,b=1;
var a=1,b=2;

let a;
let a=1;
let a,b;
let a=1,b;
let a,b=1;
let a=1,b=2;

const a=1;
const a=1,b=2;

var a={a:1,b:2};

var a={a:1,b:2},b={x:1,y:2},c=[1,2,3],d=/regex/,e=true;

// Destructuring

var {alpha:a,beta:{b,gamma:c},club:[d,e]} = o;
var {alpha:a,beta:{b,gamma:c},club:[d,e]} = {alpha:1,beta:{b:2,gamma:3},club:[4,5]};
