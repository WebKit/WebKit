import "news-site-css/dist/variables.css";
import "news-site-css/dist/global.css";
import "news-site-css/dist/a11y.css";
import "news-site-css/dist/icons.css";
import "news-site-css/dist/text.css";

import { useEffect, useState } from "react";
import Script from "next/script";

function App({ Component, pageProps }) {
    const [render, setRender] = useState(false);
    useEffect(() => setRender(true), []);
    return render
        ? <>
            <Script id="raf-mock">{`// This hack allows to capture the work normally happening in a rAF. We
// may be able to remove it if the runner improves.
window.requestAnimationFrame = (cb) => window.setTimeout(cb, 0);
window.cancelAnimationFrame = window.clearTimeout;
// Disable requestIdleCallback until WebKit / Safari supports it.
window.requestIdleCallback = undefined;
window.cancelIdleCallback = undefined;`}</Script>
            <Component {...pageProps} />
        </>
        : null;
}
export default App;
