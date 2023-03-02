// META: script=/common/utils.js
// META: script=resources/constants.sub.js

test(() => {
    const imageURL = new URL("resources/blank.png?" + token(), window.location);
    imageURL.host = CROSS_ORIGIN_HOST;

    const params = new URLSearchParams();
    params.set("csp", `allow-all`);
    params.set("preconnect_origin", `https://${CROSS_ORIGIN_HOST}`);
    params.set("image_url", imageURL.href);

    documentURL = new URL("resources/preconnect.h2.py?" + params, window.location);
    window.location.replace(documentURL);
});
