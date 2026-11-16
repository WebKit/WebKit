// This test should not fail exception check validation.

let s = new String();
s.toString = ()=>{}
JSON.stringify(s);
