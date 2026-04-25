{ 
let isSameOrigin = true;
try { top.name } catch (e) { isSameOrigin = false; }
if (isSameOrigin)
    alert("PASS");
else
    console.log("PASS");
}
