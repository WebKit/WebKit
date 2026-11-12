const shadowContent = {
    textFallback: '<slot style="display: block">Fallback text</slot>',
    contentsFallback: '<slot><span style="display: contents"><span class="box fallback"></span></span></slot>',
};

function attachShadows()
{
    for (const host of document.querySelectorAll("div:not(details > div)"))
        host.attachShadow({ mode: "open" }).innerHTML = shadowContent[host.id] ?? '<slot name="first"><span class="box fallback"></span></slot><slot><span class="box fallback"></span></slot>';
}
