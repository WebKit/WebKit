// MIT License

// Copyright (c) 2013 Gorgi Kosev

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Copyright 2018 Google LLC, Benedikt Meurer
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     <https://www.apache.org/licenses/LICENSE-2.0>
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

(function(){function r(e,n,t){function o(i,f){if(!n[i]){if(!e[i]){var c="function"==typeof require&&require;if(!f&&c)return c(i,!0);if(u)return u(i,!0);var a=new Error("Cannot find module '"+i+"'");throw a.code="MODULE_NOT_FOUND",a}var p=n[i]={exports:{}};e[i][0].call(p.exports,function(r){var n=e[i][1][r];return o(n||r)},p,p.exports,r,e,n,t)}return n[i].exports}for(var u="function"==typeof require&&require,i=0;i<t.length;i++)o(t[i]);return o}return r})()({1:[function(require,module,exports){
"use strict";

const fakes = require("../lib/fakes-async.js");

module.exports = async function doxbee(stream, idOrPath) {
  const blob = fakes.blobManager.create(fakes.account);
  const tx = fakes.db.begin();

  try {
    const blobId = await blob.put(stream);
    const file = await fakes.self.byUuidOrPath(idOrPath).get();
    const previousId = file ? file.version : null;
    const version = {
      userAccountId: fakes.userAccount.id,
      date: new Date(),
      blobId: blobId,
      creatorId: fakes.userAccount.id,
      previousId: previousId
    };
    version.id = fakes.Version.createHash(version);
    await fakes.Version.insert(version).execWithin(tx);

    let fileId;
    if (!file) {
      const splitPath = idOrPath.split("/");
      const fileName = splitPath[splitPath.length - 1];
      fileId = fakes.uuid.v1();
      const q = await fakes.self.createQuery(idOrPath, {
        id: fileId,
        userAccountId: fakes.userAccount.id,
        name: fileName,
        version: version.id
      });
      await q.execWithin(tx);
    } else {
      fileId = file.id;
    }
    await fakes.FileVersion.insert({
      fileId: fileId,
      versionId: version.id
    }).execWithin(tx);

    await fakes.File.whereUpdate(
      { id: fileId },
      { version: version.id }
    ).execWithin(tx);
    await tx.commit();
  } catch (err) {
    await tx.rollback();
    throw err;
  }
};

},{"../lib/fakes-async.js":2}],2:[function(require,module,exports){
"use strict";

async function dummy_1() { }
async function dummy_2(a) { }

// a queryish object with all kinds of functions
function Queryish() {}
Queryish.prototype.all = dummy_1;
Queryish.prototype.exec = dummy_1;
Queryish.prototype.execWithin = dummy_2;
Queryish.prototype.get = dummy_1;
function queryish() {
  return new Queryish();
}

class Uuid {
  v1() {}
}
const uuid = new Uuid();

const userAccount = { id: 1 };

const account = {};

function Blob() {}
Blob.prototype.put = dummy_2;
class BlobManager {
  create() {
    return new Blob();
  }
}
const blobManager = new BlobManager();

var cqQueryish = queryish();

function Self() {}
Self.prototype.byUuidOrPath = queryish;
Self.prototype.createQuery = async function createQuery(x, y) { return cqQueryish; };
const self = new Self();

function File() {}
File.insert = queryish;
File.whereUpdate = queryish;

function FileVersion() {}
FileVersion.insert = queryish;

function Version() {}
Version.createHash = function createHash(v) {
  return 1;
};
Version.insert = queryish;

function Transaction() {}
Transaction.prototype.commit = dummy_1;
Transaction.prototype.rollback = dummy_1;

class Db {
  begin() {
    return new Transaction();
  }
}
const db = new Db();

module.exports = {
  uuid,
  userAccount,
  account,
  blobManager,
  self,
  File,
  FileVersion,
  Version,
  db
};

},{}],3:[function(require,module,exports){
const doxbee = require("../lib/doxbee-async");

globalThis.Benchmark = class {
  runIteration() {
    const promises = new Array(10_000);

    for (var i = 0; i < 10_000; i++)
      promises[i] = doxbee(i, "foo");

    return Promise.all(promises);
  }
};

},{"../lib/doxbee-async":1}]},{},[3]);
