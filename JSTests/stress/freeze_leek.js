var o = Object.freeze([]),
    leak = {};

try { 
  throw o; 
} catch (ex) {}

if(o.stack !== undefined)
    throw new Error("the stack was leaked.");

o.stack = leak;

if(o.stack === leak)
    throw new Error("the object wasn't frozen.");

o.other = "wrong";

if(o.other === "wrong")
    throw new Error("the object wasn't frozen.");


o = Object.freeze({"hi": "other"});

try { 
  throw o; 
} catch (ex) {}
o.stack = leak;


if(o.stack !== undefined)
    throw new Error("the stack was leaked.");

o.stack = leak;

if(o.stack === leak)
    throw new Error("the object wasn't frozen.");

o.other = "wrong";

if(o.other === "wrong")
    throw new Error("the object wasn't frozen.");
