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
                throw "Expected " + player.name + " to have " + this._handTypeCounts[handTypeIdx] + " " + this._handTypes[handTypeIdx] + " hands, but they have " + actualHandTypeCounts[handTypeIdx];
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

playerExpectations.push(new PlayerExpectation(9944, [ 20065, 16861, 1875, 898, 161, 75, 58, 7, 0, 0]));
playerExpectations.push(new PlayerExpectation(9969, [ 20142, 16782, 1891, 918, 131, 61, 60, 13, 2, 0]));
playerExpectations.push(new PlayerExpectation(10028, [ 20123, 16816, 1936, 828, 152, 74, 54, 16, 1, 0]));
playerExpectations.push(new PlayerExpectation(10062, [ 20082, 16801, 1933, 874, 169, 70, 61, 9, 1, 0]));
