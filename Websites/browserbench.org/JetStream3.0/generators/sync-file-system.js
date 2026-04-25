/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

"use strict";

function computeIsLittleEndian() {
    let buf = new ArrayBuffer(4);
    let dv = new DataView(buf);
    dv.setUint32(0, 0x11223344, true);
    let view = new Uint8Array(buf);
    return view[0] === 0x44;
}

const isLittleEndian = computeIsLittleEndian();

function *randomFileContents() {
    let counter = 1;
    while(true) {
        const numBytes = (((counter * 1192.18851371) | 0) % 2056);
        counter++;
        let result = new ArrayBuffer(numBytes);
        let view = new Uint8Array(result);
        for (let i = 0; i < numBytes; ++i)
            view[i] = (i + counter) % 256;
        yield new DataView(result);
    }
};


class File {
    constructor(dataView, permissions) {
        this._data = dataView;
    }

    get data() { return this._data; }

    set data(dataView) { this._data = dataView; }

    get byteLength() { return this._data.byteLength; }

    swapByteOrder() {
        let hash = 0x1a2b3c4d;
        for (let i = 0; i < Math.floor(this.data.byteLength / 8) * 8; i += 8) {
            const data = this.data.getFloat64(i, isLittleEndian);
            this.data.setFloat64(i, data, !isLittleEndian);
            hash ^= data | 0;
        }
        return hash;
    }
}

class Directory {
    constructor() {
        this.structure = new Map;
    }

    addFile(name, file) {
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

    addDirectory(name, directory = new Directory) {
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

    * ls() {
        for (let [name, entry] of this.structure)
            yield { name, entry, isDirectory: entry instanceof Directory };
    }

    * forEachFile() {
        for (let item of this.ls()) {
            if (!item.isDirectory)
                yield item;
        }
    }

    * forEachFileRecursively() {
        for (let item of this.ls()) {
            if (item.isDirectory)
                yield* item.entry.forEachFileRecursively();
            else
                yield item;
        } 
    }

    * forEachDirectoryRecursively() {
        for (let item of this.ls()) {
            if (!item.isDirectory)
                continue;

            yield* item.entry.forEachDirectoryRecursively();
            yield item;
        } 
    }

    fileCount() {
        let count = 0;
        for (let item of this.ls()) {
            if (!item.isDirectory)
                ++count;
        }

        return count;
    }

    totalFileSize() {
        let size = 0;
        for (const { entry:file } of this.forEachFileRecursively()) {
            size += file.byteLength;
        }
        return size;
    }

    rm(name) {
        return this.structure.delete(name);
    }

    visit(visitor) {
        visitor.visitDir(undefined, this);
        for (const {name, entry, isDirectory} of this.ls()) {
            if (isDirectory)
                entry.visit(visitor);
            else
                visitor.visitFile(name, entry);
        }
    }

}

const MAX_DIR_COUNT = 5000;
const MAX_FILE_COUNT = 1200;

function setupDirectory() {
    const fs = new Directory;
    let dirs = [fs];
    for (let dir of dirs) {
        for (let i = 0; i < 15; ++i) {
            if (dirs.length < MAX_DIR_COUNT) {
                dirs.push(dir.addDirectory(`dir-${dirs.length}`));
            }
        }
    }

    let fileCounter = 0;
    for (const fileContents of randomFileContents()) {
        const dirIndex = fileCounter * 107;
        const dir = dirs[dirIndex % dirs.length];
        dir.addFile(`file-${fileCounter}`, new File(fileContents));
        fileCounter++
        if (fileCounter >= MAX_FILE_COUNT)
            break;
    }
    return fs;
}

class FSVisitor {
    visitFile(name, file) {
    }

    visitDir(name, dir) {
    }
}

class CountVisitor extends FSVisitor {
    fileCount = 0;
    dirCount = 0;

    visitFile() {
        this.fileCount++;
    }

    visitDir() {
        this.dirCount++;
    }
}

class CountDracula extends FSVisitor {
    bytes = 0;
    visitFile(name, file) {
        this.bytes += file.byteLength;
    }
}

class Benchmark {
    EXPECTED_FILE_COUNT = 1108;

    totalFileCount = 0;
    totalDirCount = 0;
    lastFileHash = undefined;
    fs;

    prepareForNextIteration() {
    }

    runIteration() {
        this.fs = setupDirectory();

        for (let { entry: file } of this.fs.forEachFileRecursively()) {
            this.lastFileHash = file.swapByteOrder();
        }

        let bytesDeleted = 0;
        let counter = 0;
        for (const { name, entry: dir } of this.fs.forEachDirectoryRecursively()) {
            const oldTotalSize = dir.totalFileSize();
            if (dir.fileCount() === 0)
                continue;
            counter++;
            if (counter % 13 !== 0)
                continue;
            for (const { name } of dir.forEachFile()) {
                const result = dir.rm(name);
                if (!result)
                    throw new Error("rm should have returned true");
            }
            const totalReducedSize = oldTotalSize - dir.totalFileSize();
            bytesDeleted += totalReducedSize;
        }
        if (bytesDeleted === 0)
            throw new Error("Did not delete any files");

        const countVisitor = new CountVisitor();
        this.fs.visit(countVisitor);

        const countDracula = new CountDracula();
        this.fs.visit(countDracula);

        let fileCount = 0;
        let fileBytes = 0;
        for (const {entry: file} of this.fs.forEachFileRecursively()) {
            fileCount++
            fileBytes += file.byteLength;
        }
        this.totalFileCount += fileCount;

        let dirCount = 1;
        for (let _ of this.fs.forEachDirectoryRecursively()) {
            dirCount++;
        }
        this.totalDirCount += dirCount;

        if (countVisitor.fileCount !== fileCount)
            throw new Error(`Invalid total file count ${countVisitor.fileCount}, expected ${fileCount}.`);
        if (countDracula.bytes !== fileBytes)
            throw new Error(`Invalid total file bytes ${countDracula.bytes}, expected ${fileBytes}.`);
        if (countVisitor.dirCount !== dirCount)
            throw new Error(`Invalid total dir count ${countVisitor.dirCount}, expected ${dirCount}.`);

    }

    validate(iterations) {
        const expectedFileCount = this.EXPECTED_FILE_COUNT * iterations;
        if (this.totalFileCount != expectedFileCount)
            throw new Error(`Invalid total file count ${this.totalFileCount}, expected ${expectedFileCount}.`);
        if (this.lastFileHash === undefined)
            throw new Error(`Invalid file hash: ${this.lastFileHash}`);
    }
}
