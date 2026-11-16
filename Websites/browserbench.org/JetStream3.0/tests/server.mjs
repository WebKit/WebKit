/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright 2025 Google LLC
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// Simple local server
import * as path from "path";
import commandLineArgs from "command-line-args";
import esMain from "es-main";
import LocalWebServer from "local-web-server";

const ROOT_DIR = path.join(process.cwd(), "./");

export const optionDefinitions = [
    {
        name: "port",
        type: Number,
        defaultValue: 8010,
        description: "Set the test-server port, The default value is 8010.",
    },
    {
        name: "quiet",
        alias: "q",
        type: Boolean,
        defaultValue: false,
        description: "Silence the server output.",
    },
];

export async function serve({ port, quiet }) {
    if (!port) throw new Error("Port is required");

    const ws = await LocalWebServer.create({
        port: port,
        directory: ROOT_DIR,
        corsOpenerPolicy: "same-origin",
        corsEmbedderPolicy: "require-corp",
        logFormat: quiet ? undefined : "dev",
    });
    console.log(`Server started on http://localhost:${port}`);
    process.on("exit", () => ws.server.close());
    return {
        close() {
            ws.server.close();
        },
    };
}

function main() {
    const options = commandLineArgs(optionDefinitions);
    serve(options);
}

if (esMain(import.meta)) {
    main();
}
