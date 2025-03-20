import { Html, Head, Main, NextScript } from "next/document";

export default function Document() {
    return (
        <Html lang="en">
            <Head />
            <body>
                <Main />
                <div id="settings-container"></div>
                <div id="notifications-container"></div>
                <NextScript />
            </body>
        </Html>
    );
}
