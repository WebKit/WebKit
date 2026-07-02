/*
 * Copyright (C) 2025 Mozilla Foundation. All rights reserved.
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


// zlib-based  utility for use in shells where CompressionStream and
// DecompressionStream are not available.

function module() {
  "use strict";

  let zlibPromise = null;
  let zlibModule = null;

  async function initialize() {
    if (zlibPromise) {
      zlibModule = await zlibPromise;
      return zlibModule;
    }
    load('wasm/zlib/build/zlib.js');
    zlibPromise = setupModule({
      wasmBinary: new Int8Array(read('wasm/zlib/build/zlib.wasm', "binary")),
    });
    zlibModule = await zlibPromise;
    return zlibModule;
  }

  function decompress(bytes) {
    zlibModule.FS.writeFile('in', bytes);
    const inputzStr = zlibModule.stringToNewUTF8('in');
    const inputzoutStr = zlibModule.stringToNewUTF8('out');
    if (zlibModule._decompressFile(inputzStr, inputzoutStr) !== 0) {
      throw new Error();
    }
    const output = zlibModule.FS.readFile('out');
    zlibModule._free(inputzStr);
    zlibModule._free(inputzoutStr);
    return output;
  }

  return {
    initialize: initialize,
    decompress: decompress,
  };
}

globalThis.zlib = module();

