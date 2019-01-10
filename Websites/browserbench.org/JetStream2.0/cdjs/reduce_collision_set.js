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

var drawMotionOnVoxelMap = (function() {
    var voxelSize = Constants.GOOD_VOXEL_SIZE;
    var horizontal = new Vector2D(voxelSize, 0);
    var vertical = new Vector2D(0, voxelSize);
    
    function voxelHash(position) {
        var xDiv = (position.x / voxelSize) | 0;
        var yDiv = (position.y / voxelSize) | 0;
        
        var result = new Vector2D();
        result.x = voxelSize * xDiv;
        result.y = voxelSize * yDiv;
        
        if (position.x < 0)
            result.x -= voxelSize;
        if (position.y < 0)
            result.y -= voxelSize;
        
        return result;
    }
    
    return function(voxelMap, motion) {
        var seen = new RedBlackTree();
        
        function putIntoMap(voxel) {
            var array = voxelMap.get(voxel);
            if (!array)
                voxelMap.put(voxel, array = []);
            array.push(motion);
        }
        
        function isInVoxel(voxel) {
            if (voxel.x > Constants.MAX_X ||
                voxel.x < Constants.MIN_X ||
                voxel.y > Constants.MAX_Y ||
                voxel.y < Constants.MIN_Y)
                return false;
            
            var init = motion.posOne;
            var fin = motion.posTwo;
            
            var v_s = voxelSize;
            var r = Constants.PROXIMITY_RADIUS / 2;
            
            var v_x = voxel.x;
            var x0 = init.x;
            var xv = fin.x - init.x;
            
            var v_y = voxel.y;
            var y0 = init.y;
            var yv = fin.y - init.y;
            
            var low_x, high_x;
            low_x = (v_x - r - x0) / xv;
            high_x = (v_x + v_s + r - x0) / xv;
            
            if (xv < 0) {
                var tmp = low_x;
                low_x = high_x;
                high_x = tmp;
            }
            
            var low_y, high_y;
            low_y = (v_y - r - y0) / yv;
            high_y = (v_y + v_s + r - y0) / yv;
            
            if (yv < 0) {
                var tmp = low_y;
                low_y = high_y;
                high_y = tmp;
            }
            
            if (false) {
                print("v_x = " + v_x + ", x0 = " + x0 + ", xv = " + xv + ", v_y = " + v_y + ", y0 = " + y0 + ", yv = " + yv + ", low_x = " + low_x + ", low_y = " + low_y + ", high_x = " + high_x + ", high_y = " + high_y);
            }
            
            return (((xv == 0 && v_x <= x0 + r && x0 - r <= v_x + v_s) /* no motion in x */ || 
                     ((low_x <= 1 && 1 <= high_x) || (low_x <= 0 && 0 <= high_x) ||
                      (0 <= low_x && high_x <= 1))) && 
                    ((yv == 0 && v_y <= y0 + r && y0 - r <= v_y + v_s) /* no motion in y */ || 
                     ((low_y <= 1 && 1 <= high_y) || (low_y <= 0 && 0 <= high_y) ||
                      (0 <= low_y && high_y <= 1))) && 
                    (xv == 0 || yv == 0 || /* no motion in x or y or both */
                     (low_y <= high_x && high_x <= high_y) ||
                     (low_y <= low_x && low_x <= high_y) ||
                     (low_x <= low_y && high_y <= high_x)));
        }
        
        function recurse(nextVoxel) {
            if (!isInVoxel(nextVoxel, motion))
                return;
            if (seen.put(nextVoxel, true))
                return;
            
            putIntoMap(nextVoxel);
            
            recurse(nextVoxel.minus(horizontal));
            recurse(nextVoxel.plus(horizontal));
            recurse(nextVoxel.minus(vertical));
            recurse(nextVoxel.plus(vertical));
            recurse(nextVoxel.minus(horizontal).minus(vertical));
            recurse(nextVoxel.minus(horizontal).plus(vertical));
            recurse(nextVoxel.plus(horizontal).minus(vertical));
            recurse(nextVoxel.plus(horizontal).plus(vertical));
        }
        
        recurse(voxelHash(motion.posOne));
    };
})();

function reduceCollisionSet(motions) {
    var voxelMap = new RedBlackTree();
    for (var i = 0; i < motions.length; ++i)
        drawMotionOnVoxelMap(voxelMap, motions[i]);
        
    var result = [];
    voxelMap.forEach(function(key, value) {
        if (value.length > 1)
            result.push(value);
    });
    return result;
}

