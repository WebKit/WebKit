// Simple server adapted from https://developer.mozilla.org/en-US/docs/Learn/Server-side/Node_server_without_framework:
import * as fs from "fs";
import * as http from "http";
import * as path from "path";
const MIME_TYPES = {
    default: "application/octet-stream",
    html: "text/html; charset=UTF-8",
    js: "application/javascript; charset=UTF-8",
    mjs: "application/javascript; charset=UTF-8",
    css: "text/css",
    png: "image/png",
    jpg: "image/jpg",
    gif: "image/gif",
    ico: "image/x-icon",
    svg: "image/svg+xml",
};

const STATIC_PATH = path.join(process.cwd(), "./");
const toBool = [() => true, () => false];

export default function serve(port) {
    if (!port)
        throw new Error("Port is required");

    const prepareFile = async (url) => {
        const paths = [STATIC_PATH, url];
        if (url.endsWith("/"))
            paths.push("index.html");
        const filePath = path.join(...paths);
        const pathTraversal = !filePath.startsWith(STATIC_PATH);
        const exists = await fs.promises.access(filePath).then(...toBool);
        const found = !pathTraversal && exists;
        const streamPath = found ? filePath : `${STATIC_PATH}/index.html`;
        const ext = path.extname(streamPath).substring(1).toLowerCase();
        const stream = fs.createReadStream(streamPath);
        return { found, ext, stream };
    };

    const server = http
        .createServer(async (req, res) => {
            const file = await prepareFile(req.url);
            const statusCode = file.found ? 200 : 404;
            const mimeType = MIME_TYPES[file.ext] || MIME_TYPES.default;
            res.writeHead(statusCode, { "Content-Type": mimeType });
            file.stream.pipe(res);
        })
        .listen(port);

    console.log(`Server running at http://127.0.0.1:${port}/`);
    return server;
}
