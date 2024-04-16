// Copyright (c) 2001-2010, Purdue University. All rights reserved.
// Copyright (C) 2015-2016 Apple Inc. All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of the Purdue University nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

function benchmarkImpl(configuration) {
    var verbosity = configuration.verbosity;
    var numAircraft = configuration.numAircraft;
    var numFrames = configuration.numFrames;
    var expectedCollisions = configuration.expectedCollisions;
    var exclude = configuration.exclude;

    var simulator = new Simulator(numAircraft);
    var detector = new CollisionDetector();
    var lastTime = currentTime();
    var results = [];
    for (var i = 0; i < numFrames; ++i) {
        var time = i / 10;
        
        var collisions = detector.handleNewFrame(simulator.simulate(time));
        
        var before = lastTime;
        var after = currentTime();
        lastTime = after;
        var result = {
            time: after - before,
            numCollisions: collisions.length
        };
        if (verbosity >= 2)
            print("CDjs: " + result.time);
        if (verbosity >= 3)
            result.collisions = collisions;
        results.push(result);
    }
    
    results.splice(0, exclude);

    if (verbosity >= 1) {
        for (var i = 0; i < results.length; ++i) {
            var string = "Frame " + i + ": " + results[i].time + " ms.";
            if (results[i].numCollisions)
                string += " (" + results[i].numCollisions + " collisions.)";
            print(string);
            if (verbosity >= 2 && results[i].collisions.length)
                print("    Collisions: " + results[i].collisions);
        }
    }

    // Check results.
    var actualCollisions = 0;
    for (var i = 0; i < results.length; ++i)
        actualCollisions += results[i].numCollisions;
    if (actualCollisions != expectedCollisions) {
        throw new Error("Bad number of collisions: " + actualCollisions + " (expected " + expectedCollisions + ")");
    }
}

function benchmark() {
    return benchmarkImpl({
        verbosity: 0,
        numAircraft: 1000,
        numFrames: 18,
        expectedCollisions: 1336,
        exclude: 0
    });
}

class Benchmark {
    runIteration() {
        benchmark();
    }
}
