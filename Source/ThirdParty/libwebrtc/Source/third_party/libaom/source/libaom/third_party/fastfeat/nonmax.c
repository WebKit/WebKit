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
#include <assert.h>
#include <stdlib.h>
#include "fast.h"


#define Compare(X, Y) ((X)>=(Y))

xy* aom_nonmax_suppression(const xy* corners, const int* scores, int num_corners,
                           int** ret_scores, int* ret_num_nonmax)
{
  int num_nonmax=0;
  int last_row;
  int* row_start;
  int i, j;
  xy* ret_nonmax;
  int* nonmax_scores;
  const int sz = (int)num_corners;

  /*Point above points (roughly) to the pixel above the one of interest, if there
    is a feature there.*/
  int point_above = 0;
  int point_below = 0;

  *ret_scores = 0;
  *ret_num_nonmax = -1;
  if(!(corners && scores) || num_corners < 1)
  {
    *ret_num_nonmax = 0;
    return 0;
  }

  ret_nonmax = (xy*)malloc(num_corners * sizeof(xy));
  if(!ret_nonmax)
  {
    return 0;
  }

  nonmax_scores = (int*)malloc(num_corners * sizeof(*nonmax_scores));
  if (!nonmax_scores)
  {
    free(ret_nonmax);
    return 0;
  }

  /* Find where each row begins
     (the corners are output in raster scan order). A beginning of -1 signifies
     that there are no corners on that row. */
  last_row = corners[num_corners-1].y;
  row_start = (int*)malloc((last_row+1)*sizeof(int));
  if(!row_start)
  {
    free(ret_nonmax);
    free(nonmax_scores);
    return 0;
  }

  for(i=0; i < last_row+1; i++)
    row_start[i] = -1;

  {
    int prev_row = -1;
    for(i=0; i< num_corners; i++)
      if(corners[i].y != prev_row)
      {
        row_start[corners[i].y] = i;
        prev_row = corners[i].y;
      }
  }



  for(i=0; i < sz; i++)
  {
    int score = scores[i];
    xy pos = corners[i];
    assert(pos.y <= last_row);

    /*Check left */
    if(i > 0)
      if(corners[i-1].x == pos.x-1 && corners[i-1].y == pos.y && Compare(scores[i-1], score))
        continue;

    /*Check right*/
    if(i < (sz - 1))
      if(corners[i+1].x == pos.x+1 && corners[i+1].y == pos.y && Compare(scores[i+1], score))
        continue;

    /*Check above (if there is a valid row above)*/
    if(pos.y > 0 && row_start[pos.y - 1] != -1)
    {
      /*Make sure that current point_above is one
        row above.*/
      if(corners[point_above].y < pos.y - 1)
        point_above = row_start[pos.y-1];

      /*Make point_above point to the first of the pixels above the current point,
        if it exists.*/
      for(; corners[point_above].y < pos.y && corners[point_above].x < pos.x - 1; point_above++)
      {}


      for(j=point_above; corners[j].y < pos.y && corners[j].x <= pos.x + 1; j++)
      {
        int x = corners[j].x;
        if( (x == pos.x - 1 || x ==pos.x || x == pos.x+1) && Compare(scores[j], score))
          goto cont;
      }

    }

    /*Check below (if there is anything below)*/
    if (pos.y + 1 < last_row+1 && row_start[pos.y + 1] != -1 && point_below < sz) /*Nothing below*/
    {
      if(corners[point_below].y < pos.y + 1)
        point_below = row_start[pos.y+1];

      /* Make point below point to one of the pixels belowthe current point, if it
         exists.*/
      for(; point_below < sz && corners[point_below].y == pos.y+1 && corners[point_below].x < pos.x - 1; point_below++)
      {}

      for(j=point_below; j < sz && corners[j].y == pos.y+1 && corners[j].x <= pos.x + 1; j++)
      {
        int x = corners[j].x;
        if( (x == pos.x - 1 || x ==pos.x || x == pos.x+1) && Compare(scores[j],score))
          goto cont;
      }
    }

    ret_nonmax[num_nonmax] = corners[i];
    nonmax_scores[num_nonmax] = scores[i];
    num_nonmax++;
cont:
    ;
  }

  free(row_start);
  *ret_scores = nonmax_scores;
  *ret_num_nonmax = num_nonmax;
  return ret_nonmax;
}

// clang-format on
