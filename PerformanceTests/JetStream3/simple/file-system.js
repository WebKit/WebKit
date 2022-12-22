"use strict";

/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

function computeIsLittleEndian() {
    let buf = new ArrayBuffer(4);
    let dv = new DataView(buf);
    dv.setUint32(0, 0x11223344, true);
    let view = new Uint8Array(buf);
    return view[0] === 0x44;
}

const isLittleEndian = computeIsLittleEndian();

function randomFileContents(bytes = ((Math.random() * 128) >>> 0) + 2056) {
    let result = new ArrayBuffer(bytes);
    let view = new Uint8Array(result);
    for (let i = 0; i < bytes; ++i)
        view[i] = (Math.random() * 255) >>> 0;
    return new DataView(result);
}

class File {
    constructor(dataView, permissions) {
        this._data = dataView;
    }

    get data() { return this._data; }

    set data(dataView) { this._data = dataView; }

    swapByteOrder() {
        for (let i = 0; i < Math.floor(this.data.byteLength / 8) * 8; i += 8) {
            this.data.setFloat64(i, this.data.getFloat64(i, isLittleEndian), !isLittleEndian);
        }
    }
}

class Directory {
    constructor() {
        this.structure = new Map;
    }

    async addFile(name, file) {
        let entry = this.structure.get(name);
        if (entry !== undefined) {
            if (entry instanceof File)
                throw new Error("Can't replace file with file.");
            if (entry instanceof Directory)
                throw new Error("Can't replace a file with a new directory.");
            throw new Error("Should not reach this code");
        }

        this.structure.set(name, file);
        return file;
    }

    async addDirectory(name, directory = new Directory) {
        let entry = this.structure.get(name);
        if (entry !== undefined) {
            if (entry instanceof File)
                throw new Error("Can't replace file with directory.");
            if (entry instanceof Directory)
                throw new Error("Can't replace directory with new directory.");
            throw new Error("Should not reach this code");
        }

        this.structure.set(name, directory);
        return directory;
    }

    async* ls() {
        for (let [name, entry] of this.structure)
            yield { name, entry, isDirectory: entry instanceof Directory };
    }

    async* forEachFile() {
        for await (let item of this.ls()) {
            if (!item.isDirectory)
                yield item;
        }
    }

    async* forEachFileRecursively() {
        for await (let item of this.ls()) {
            if (item.isDirectory) {
                for await (let file of item.entry.forEachFileRecursively())
                    yield file;
            } else {
                yield item;
            }
        } 
    }

    async* forEachDirectoryRecursively() {
        for await (let item of this.ls()) {
            if (!item.isDirectory)
                continue;

            for await (let dirItem of item.entry.forEachDirectoryRecursively())
                yield dirItem;

            yield item;
        } 
    }

    async fileCount() {
        let count = 0;
        for await (let item of this.ls()) {
            if (!item.isDirectory)
                ++count;
        }

        return count;
    }

    async rm(name) {
        return this.structure.delete(name);
    }
}

async function setupDirectory() {
    const fs = new Directory;
    let dirs = [fs];
    for (let dir of dirs) {
        for (let i = 0; i < 8; ++i) {
            if (dirs.length < 250 && Math.random() >= 0.3) {
                dirs.push(await dir.addDirectory(`dir-${i}`));
            }
        }
    }

    for (let dir of dirs) {
        for (let i = 0; i < 5; ++i) {
            if (Math.random() >= 0.6) {
                await dir.addFile(`file-${i}`, new File(randomFileContents()));
            }
        }
    }

    return fs;
}

class Benchmark {
    async runIteration() {
        try {
            const fs = await setupDirectory();

            for await (let { entry: file } of fs.forEachFileRecursively()) {
                file.swapByteOrder();
            }

            for await (let { name, entry: dir } of fs.forEachDirectoryRecursively()) {
                if ((await dir.fileCount()) > 3) {
                    for await (let { name } of dir.forEachFile()) {
                        let result = await dir.rm(name);
                        if (!result)
                            throw new Error("rm should have returned true");
                        
                    }
                }
            }
        } catch(e) {
            console.log("Error running benchmark: ", e, e.line);
        }
    }
}
