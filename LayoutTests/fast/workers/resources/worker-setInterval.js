function onmessage(evt)
{
    postMessage("SUCCESS");
}

let i = 0;
setInterval(() => {
    postMessage("" + i);
    i++;
}, 0);

addEventListener("message", onmessage, true);
