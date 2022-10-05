// Copyright (c) 2001-2010, Purdue University. All rights reserved.
// Copyright (C) 2015 Apple Inc. All rights reserved.
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

function CollisionDetector() {
    this._state = new RedBlackTree();
}

CollisionDetector.prototype.handleNewFrame = function(frame) {
    var motions = [];
    var seen = new RedBlackTree();
    
    for (var i = 0; i < frame.length; ++i) {
        var aircraft = frame[i];
        
        var oldPosition = this._state.put(aircraft.callsign, aircraft.position);
        var newPosition = aircraft.position;
        seen.put(aircraft.callsign, true);
        
        if (!oldPosition) {
            // Treat newly introduced aircraft as if they were stationary.
            oldPosition = newPosition;
        }
        
        motions.push(new Motion(aircraft.callsign, oldPosition, newPosition));
    }
    
    // Remove aircraft that are no longer present.
    var toRemove = [];
    this._state.forEach(function(callsign, position) {
        if (!seen.get(callsign))
            toRemove.push(callsign);
    });
    for (var i = 0; i < toRemove.length; ++i)
        this._state.remove(toRemove[i]);
    
    var allReduced = reduceCollisionSet(motions);
    var collisions = [];
    for (var reductionIndex = 0; reductionIndex < allReduced.length; ++reductionIndex) {
        var reduced = allReduced[reductionIndex];
        for (var i = 0; i < reduced.length; ++i) {
            var motion1 = reduced[i];
            for (var j = i + 1; j < reduced.length; ++j) {
                var motion2 = reduced[j];
                var collision = motion1.findIntersection(motion2);
                if (collision)
                    collisions.push(new Collision([motion1.callsign, motion2.callsign], collision));
            }
        }
    }
    
    return collisions;
};
