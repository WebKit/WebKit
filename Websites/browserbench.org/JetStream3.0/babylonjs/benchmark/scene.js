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

// console.log = () => {};

globalThis.setTimeout = (callback, timeout) => callback();
globalThis.requestAnimationFrame = (callback) => callback();

// JetStream benchmark.
class Benchmark {
  iterationCount = 0;
  preloaded = {
    fortData: null,
    cannonData: null,
    particlesJson: null,
  };

  constructor(iterationCount) {
    this.iterationCount = iterationCount;
  }

  async init() {
    const [fort, cannon, particles] = await Promise.all([
      JetStream.getBinary(JetStream.preload.PIRATE_FORT_BLOB),
      JetStream.getBinary(JetStream.preload.CANNON_BLOB),
      JetStream.getString(JetStream.preload.PARTICLES_BLOB),
    ]);
    this.preloaded.fortData = fort;
    this.preloaded.cannonData = cannon;
    this.preloaded.particlesJson = JSON.parse(particles);
  }

  async runIteration() {
    const {classNames, cameraRotationLength} = await BabylonJSBenchmark.runComplexScene(
      this.preloaded.fortData,
      this.preloaded.cannonData,
      this.preloaded.particlesJson,
      100
    );
    const lastResult = {
      classNames,
      cameraRotationLength
    };
    this.validateIteration(lastResult);
  }

  validateIteration(lastResult) {
    this.expect("this.lastResult.classNames.length", lastResult.classNames.length, 2135);
    this.expect("this.lastResult.cameraRotationLength", lastResult.cameraRotationLength, 0);
  }

  expect(name, value, expected) {
    if (value != expected)
      throw new Error(`Expected ${name} to be ${expected}, but got ${value}`);
  }
}
