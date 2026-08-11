// Copyright (C) 2007-2025 Apple Inc. All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//  notice, this list of conditions and the following disclaimer in the
//  documentation and/or other materials provided with the distribution.

// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.


import commandLineArgs from 'command-line-args';
import commandLineUsage from 'command-line-usage';
import { globSync } from 'glob';
import zlib from 'zlib';
import fs from 'fs';
import path from 'path';

let log = console.log

function parseCommandLineArgs() {
    const optionDefinitions = [
        { name: 'decompress', alias: 'd', type: Boolean, description: 'Decompress files (default: compress).' },
        { name: 'keep', alias: 'k', type: Boolean, description: 'Keep input files after processing (default: delete).' },
        { name: 'help', alias: 'h', type: Boolean, description: 'Print this usage guide.' },
         { name: 'quiet', alias: 'q', type: Boolean, description: 'Quite output, only print errors.' },
        { name: 'globs', type: String, multiple: true, defaultOption: true, description: 'Glob patterns of files to process.' },
    ];
    const options = commandLineArgs(optionDefinitions);

    const isNPM = process.env.npm_config_user_agent !== undefined;
    const command = isNPM ? 'npm run compress --' : 'node utils/compress.mjs';
    const usage = commandLineUsage([
        {
            header: 'Usage',
            content: `${command} [options] <glob>...`
        },
        {
            header: 'Options',
            optionList: optionDefinitions
        }
    ]);

    if (options.help) {
        console.log(usage);
        process.exit(0);
    }

    if (options.quiet) {
        log = () => {};
    }

    if (options.globs === undefined) {
        if (options.decompress) {
            const defaultGlob = '**/*.z';
            log(`No input glob pattern given, using default: ${defaultGlob}`);
            options.globs = [defaultGlob];
        } else {
            // For compression, require the user to specify explicit input file patterns.
            console.error('No input glob pattern given.');
            console.log(usage);
            process.exit(1);
        }
    }
    return options;
}

function calculateCompressionRatio(originalSize, compressedSize) {
    return (1 - compressedSize / originalSize) * 100;
}

function calculateExpansionRatio(compressedSize, decompressedSize) {
    return (decompressedSize / compressedSize - 1) * 100;
}

function compress(inputData) {
    const compressedData = zlib.deflateSync(inputData, { level: zlib.constants.Z_BEST_COMPRESSION });

    const originalSize = inputData.length;
    const compressedSize = compressedData.length;
    const compressionRatio = calculateCompressionRatio(originalSize, compressedSize);
    log(`  Original size:   ${String(originalSize).padStart(8)} bytes`);
    log(`  Compressed size: ${String(compressedSize).padStart(8)} bytes`);
    log(`  Compression ratio:  ${compressionRatio.toFixed(2).padStart(8)}%`);

    return compressedData;
}

function decompress(inputData) {
    const decompressedData = zlib.inflateSync(inputData);

    const compressedSize = inputData.length;
    const decompressedSize = decompressedData.length;
    const expansionRatio = calculateExpansionRatio(compressedSize, decompressedSize);
    log(`  Compressed size:   ${String(compressedSize).padStart(8)} bytes`);
    log(`  Decompressed size: ${String(decompressedSize).padStart(8)} bytes`);
    log(`  Expansion ratio:      ${expansionRatio.toFixed(2).padStart(8)}%`);

    return decompressedData;
}

function globsToFiles(globs) {
    let files = [];
    console.assert(globs.length > 0) ;
    const globtions =  { nodir: true, ignore: '**/node_modules/**' };
    for (const glob of globs) {
        const matches = globSync(glob, globtions);
        files = files.concat(matches);
    }
    files = Array.from(new Set(files)).sort();
    return files;
}

function processFiles(files, isDecompress, keep) {
    const verb = isDecompress ? 'decompress' : 'compress';
    log(`Found ${files.length} files to ${verb}` + (files.length ? ':' : '.'));

    // For printing overall statistics at the end.
    let totalInputSize = 0;
    let totalOutputSize = 0;

    for (const inputFilename of files) {
        try {
            console.log(inputFilename);
            let outputFilename;
            if (isDecompress) {
                if (path.extname(inputFilename) !== '.z') {
                    console.warn(`  Warning: Input file does not have a .z extension.`);
                    outputFilename = `${inputFilename}.decompressed`;
                } else {
                    outputFilename = inputFilename.slice(0, -2);
                }
                log(`  Decompressing to: ${outputFilename}`);
            } else {
                if (path.extname(inputFilename) === '.z') {
                    console.warn(`  Warning: Input file already has a .z extension.`);
                }
                outputFilename = `${inputFilename}.z`;
            }

            // Copy the mode over to avoid git status entries after a roundtrip.
            const { mode } = fs.statSync(inputFilename);
            const inputData = fs.readFileSync(inputFilename);
            const outputData = isDecompress ? decompress(inputData) : compress(inputData);
            fs.writeFileSync(outputFilename, outputData, { mode });

            totalInputSize += inputData.length;
            totalOutputSize += outputData.length;

            if (!keep) {
                fs.unlinkSync(inputFilename);
                log(`  Deleted input file.`);
            }
        } catch (err) {
            console.error(`Error ${verb}ing ${inputFilename}:`, err);
        }
    }

    if (files.length > 1) {
        if (isDecompress) {
            const totalExpansionRatio = calculateExpansionRatio(totalInputSize, totalOutputSize);
            log(`Total compressed sizes:   ${String(totalInputSize).padStart(9)} bytes`);
            log(`Total decompressed sizes: ${String(totalOutputSize).padStart(9)} bytes`);
            log(`Average expansion ratio:     ${totalExpansionRatio.toFixed(2).padStart(9)}%`);
        } else {
            const totalCompressionRatio = calculateCompressionRatio(totalInputSize, totalOutputSize);
            log(`Total original sizes:   ${String(totalInputSize).padStart(9)} bytes`);
            log(`Total compressed sizes: ${String(totalOutputSize).padStart(9)} bytes`);
            log(`Average compression ratio: ${totalCompressionRatio.toFixed(2).padStart(9)}%`);
        }
    }
}

const options = parseCommandLineArgs();
const files = globsToFiles(options.globs);
processFiles(files, options.decompress, options.keep);
