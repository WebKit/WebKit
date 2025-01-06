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

class PlayerExpectation
{
    constructor(wins, handTypeCounts)
    {
        this._wins = wins;
        this._handTypeCounts = handTypeCounts;
    }

    validate(player)
    {
        if (player.wins != this._wins)
            throw "Expected " + player.name + " to have " + this._wins + ", but they have " + player.wins;

        let actualHandTypeCounts = player.handTypeCounts;
        if (this._handTypeCounts.length != actualHandTypeCounts.length)
            throw "Expected " + player.name + " to have " + this._handTypeCounts.length + " hand types, but they have " + actualHandTypeCounts.length;

        for (let handTypeIdx = 0; handTypeIdx < this._handTypeCounts.length; handTypeIdx++) {
            if (this._handTypeCounts[handTypeIdx] != actualHandTypeCounts[handTypeIdx]) {
                throw "Expected " + player.name + " to have " + this._handTypeCounts[handTypeIdx] + " " + PlayerExpectation._handTypes[handTypeIdx] + " hands, but they have " + actualHandTypeCounts[handTypeIdx];
            }

        }
    }
}

PlayerExpectation._handTypes = [
    "High Cards",
    "Pairs",
    "Two Pairs",
    "Three of a Kinds",
    "Straights",
    "Flushes",
    "Full Houses",
    "Four of a Kinds",
    "Straight Flushes",
    "Royal Flushes"
];
    
var playerExpectations = [];

playerExpectations.push(new PlayerExpectation(60120, [120600, 102000, 10080, 5040, 1440, 360, 480, 0, 0, 0]));
playerExpectations.push(new PlayerExpectation(60480, [120360, 99600, 11760, 6120, 1200, 360, 480, 120, 0, 0]));
playerExpectations.push(new PlayerExpectation(61440, [121200, 99720, 11040, 6120, 1080, 480, 360, 0, 0, 0]));
playerExpectations.push(new PlayerExpectation(57960, [121320, 100440, 11040, 5760, 840, 480, 120, 0, 0, 0]));
