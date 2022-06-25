{
let isSameOrigin = true;
try { top.name } catch (e) { isSameOrigin = false; }
if (isSameOrigin)
    alert("FAIL");
else
    console.log("FAIL");
}
