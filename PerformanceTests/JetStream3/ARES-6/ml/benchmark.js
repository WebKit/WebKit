/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

"use strict";

class Benchmark {
    constructor() { }

    runIteration()
    {
        let Matrix = MLMatrix;
        let ACTIVATION_FUNCTIONS = FeedforwardNeuralNetworksActivationFunctions;

        function run() {
            
            let it = (name, f) => {
                f();
            };

            function assert(b) {
                if (!b)
                    throw new Error("Bad");
            }

            var functions = Object.keys(ACTIVATION_FUNCTIONS);

            it('Training the neural network with XOR operator', function () {
                var trainingSet = new Matrix([[0, 0], [0, 1], [1, 0], [1, 1]]);
                var predictions = [false, true, true, false];

                for (var i = 0; i < functions.length; ++i) {
                    var options = {
                        hiddenLayers: [4],
                        iterations: 40,
                        learningRate: 0.3,
                        activation: functions[i]
                    };
                    var xorNN = new FeedforwardNeuralNetwork(options);

                    xorNN.train(trainingSet, predictions);
                    var results = xorNN.predict(trainingSet);
                }
            });

            it('Training the neural network with AND operator', function () {
                var trainingSet = [[0, 0], [0, 1], [1, 0], [1, 1]];
                var predictions = [[1, 0], [1, 0], [1, 0], [0, 1]];

                for (var i = 0; i < functions.length; ++i) {
                    var options = {
                        hiddenLayers: [3],
                        iterations: 75,
                        learningRate: 0.3,
                        activation: functions[i]
                    };
                    var andNN = new FeedforwardNeuralNetwork(options);
                    andNN.train(trainingSet, predictions);

                    var results = andNN.predict(trainingSet);
                }
            });

            it('Export and import', function () {
                var trainingSet = [[0, 0], [0, 1], [1, 0], [1, 1]];
                var predictions = [0, 1, 1, 1];

                for (var i = 0; i < functions.length; ++i) {
                    var options = {
                        hiddenLayers: [4],
                        iterations: 40,
                        learningRate: 0.3,
                        activation: functions[i]
                    };
                    var orNN = new FeedforwardNeuralNetwork(options);
                    orNN.train(trainingSet, predictions);

                    var model = JSON.parse(JSON.stringify(orNN));
                    var networkNN = FeedforwardNeuralNetwork.load(model);

                    var results = networkNN.predict(trainingSet);
                }
            });

            it('Multiclass clasification', function () {
                var trainingSet = [[0, 0], [0, 1], [1, 0], [1, 1]];
                var predictions = [2, 0, 1, 0];

                for (var i = 0; i < functions.length; ++i) {
                    var options = {
                        hiddenLayers: [4],
                        iterations: 40,
                        learningRate: 0.5,
                        activation: functions[i]
                    };
                    var nn = new FeedforwardNeuralNetwork(options);
                    nn.train(trainingSet, predictions);

                    var result = nn.predict(trainingSet);
                }
            });

            it('Big case', function () {
                var trainingSet = [[1, 1], [1, 2], [2, 1], [2, 2], [3, 1], [1, 3], [1, 4], [4, 1],
                                    [6, 1], [6, 2], [6, 3], [6, 4], [6, 5], [5, 5], [4, 5], [3, 5]];
                var predictions = [[1, 0], [1, 0], [1, 0], [1, 0], [1, 0], [1, 0], [1, 0], [1, 0],
                                    [0, 1], [0, 1], [0, 1], [0, 1], [0, 1], [0, 1], [0, 1], [0, 1]];
                for (var i = 0; i < functions.length; ++i) {
                    var options = {
                        hiddenLayers: [20],
                        iterations: 60,
                        learningRate: 0.01,
                        activation: functions[i]
                    };
                    var nn = new FeedforwardNeuralNetwork(options);
                    nn.train(trainingSet, predictions);

                    var result = nn.predict([[5, 4]]);

                    assert(result[0][0] < result[0][1]);
                }
            });
        }

        run();
    }
}
