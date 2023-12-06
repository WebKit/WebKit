// Copyright (c) 2006, 2008 Edward Rosten
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//  *Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
//  *Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
//  *Neither the name of the University of Cambridge nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// clang-format off
#include <stdlib.h>
#include "fast.h"


xy* aom_fast9_detect_nonmax(const byte* im, int xsize, int ysize, int stride, int b,
                            int** ret_scores, int* ret_num_corners)
{
  xy* corners;
  int num_corners;
  int* scores;
  xy* nonmax;

  corners = aom_fast9_detect(im, xsize, ysize, stride, b, &num_corners);
  if(!corners)
  {
    // Memory allocation failure
    *ret_num_corners = -1;
    return NULL;
  }
  // num_corners may be zero.
  scores = aom_fast9_score(im, stride, corners, num_corners, b);
  if(!scores && num_corners > 0)
  {
    // Memory allocation failure
    free(corners);
    *ret_num_corners = -1;
    return NULL;
  }
  nonmax = aom_nonmax_suppression(corners, scores, num_corners, ret_scores, ret_num_corners);

  free(corners);
  free(scores);

  return nonmax;
}
// clang-format on
