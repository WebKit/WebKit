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
/*This is mechanically generated code*/
#include <stdlib.h>
#include "fast.h"

static int fast9_corner_score(const byte* p, const int pixel[], int bstart)
{
  int bmin = bstart;
  int bmax = 255;
  int b = (bmax + bmin)/2;

  /*Compute the score using binary search*/
  for(;;)
  {
    int cb = *p + b;
    int c_b= *p - b;


    if( p[pixel[0]] > cb)
      if( p[pixel[1]] > cb)
        if( p[pixel[2]] > cb)
          if( p[pixel[3]] > cb)
            if( p[pixel[4]] > cb)
              if( p[pixel[5]] > cb)
                if( p[pixel[6]] > cb)
                  if( p[pixel[7]] > cb)
                    if( p[pixel[8]] > cb)
                      goto is_a_corner;
                    else
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        goto is_not_a_corner;
                  else if( p[pixel[7]] < c_b)
                    if( p[pixel[14]] > cb)
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        goto is_not_a_corner;
                    else if( p[pixel[14]] < c_b)
                      if( p[pixel[8]] < c_b)
                        if( p[pixel[9]] < c_b)
                          if( p[pixel[10]] < c_b)
                            if( p[pixel[11]] < c_b)
                              if( p[pixel[12]] < c_b)
                                if( p[pixel[13]] < c_b)
                                  if( p[pixel[15]] < c_b)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    if( p[pixel[14]] > cb)
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else if( p[pixel[6]] < c_b)
                  if( p[pixel[15]] > cb)
                    if( p[pixel[13]] > cb)
                      if( p[pixel[14]] > cb)
                        goto is_a_corner;
                      else
                        goto is_not_a_corner;
                    else if( p[pixel[13]] < c_b)
                      if( p[pixel[7]] < c_b)
                        if( p[pixel[8]] < c_b)
                          if( p[pixel[9]] < c_b)
                            if( p[pixel[10]] < c_b)
                              if( p[pixel[11]] < c_b)
                                if( p[pixel[12]] < c_b)
                                  if( p[pixel[14]] < c_b)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    if( p[pixel[7]] < c_b)
                      if( p[pixel[8]] < c_b)
                        if( p[pixel[9]] < c_b)
                          if( p[pixel[10]] < c_b)
                            if( p[pixel[11]] < c_b)
                              if( p[pixel[12]] < c_b)
                                if( p[pixel[13]] < c_b)
                                  if( p[pixel[14]] < c_b)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[13]] > cb)
                    if( p[pixel[14]] > cb)
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else if( p[pixel[13]] < c_b)
                    if( p[pixel[7]] < c_b)
                      if( p[pixel[8]] < c_b)
                        if( p[pixel[9]] < c_b)
                          if( p[pixel[10]] < c_b)
                            if( p[pixel[11]] < c_b)
                              if( p[pixel[12]] < c_b)
                                if( p[pixel[14]] < c_b)
                                  if( p[pixel[15]] < c_b)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else if( p[pixel[5]] < c_b)
                if( p[pixel[14]] > cb)
                  if( p[pixel[12]] > cb)
                    if( p[pixel[13]] > cb)
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              if( p[pixel[9]] > cb)
                                if( p[pixel[10]] > cb)
                                  if( p[pixel[11]] > cb)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else if( p[pixel[12]] < c_b)
                    if( p[pixel[6]] < c_b)
                      if( p[pixel[7]] < c_b)
                        if( p[pixel[8]] < c_b)
                          if( p[pixel[9]] < c_b)
                            if( p[pixel[10]] < c_b)
                              if( p[pixel[11]] < c_b)
                                if( p[pixel[13]] < c_b)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else if( p[pixel[14]] < c_b)
                  if( p[pixel[7]] < c_b)
                    if( p[pixel[8]] < c_b)
                      if( p[pixel[9]] < c_b)
                        if( p[pixel[10]] < c_b)
                          if( p[pixel[11]] < c_b)
                            if( p[pixel[12]] < c_b)
                              if( p[pixel[13]] < c_b)
                                if( p[pixel[6]] < c_b)
                                  goto is_a_corner;
                                else
                                  if( p[pixel[15]] < c_b)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  if( p[pixel[6]] < c_b)
                    if( p[pixel[7]] < c_b)
                      if( p[pixel[8]] < c_b)
                        if( p[pixel[9]] < c_b)
                          if( p[pixel[10]] < c_b)
                            if( p[pixel[11]] < c_b)
                              if( p[pixel[12]] < c_b)
                                if( p[pixel[13]] < c_b)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                if( p[pixel[12]] > cb)
                  if( p[pixel[13]] > cb)
                    if( p[pixel[14]] > cb)
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              if( p[pixel[9]] > cb)
                                if( p[pixel[10]] > cb)
                                  if( p[pixel[11]] > cb)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else if( p[pixel[12]] < c_b)
                  if( p[pixel[7]] < c_b)
                    if( p[pixel[8]] < c_b)
                      if( p[pixel[9]] < c_b)
                        if( p[pixel[10]] < c_b)
                          if( p[pixel[11]] < c_b)
                            if( p[pixel[13]] < c_b)
                              if( p[pixel[14]] < c_b)
                                if( p[pixel[6]] < c_b)
                                  goto is_a_corner;
                                else
                                  if( p[pixel[15]] < c_b)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
            else if( p[pixel[4]] < c_b)
              if( p[pixel[13]] > cb)
                if( p[pixel[11]] > cb)
                  if( p[pixel[12]] > cb)
                    if( p[pixel[14]] > cb)
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              if( p[pixel[9]] > cb)
                                if( p[pixel[10]] > cb)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              if( p[pixel[9]] > cb)
                                if( p[pixel[10]] > cb)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else if( p[pixel[11]] < c_b)
                  if( p[pixel[5]] < c_b)
                    if( p[pixel[6]] < c_b)
                      if( p[pixel[7]] < c_b)
                        if( p[pixel[8]] < c_b)
                          if( p[pixel[9]] < c_b)
                            if( p[pixel[10]] < c_b)
                              if( p[pixel[12]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else if( p[pixel[13]] < c_b)
                if( p[pixel[7]] < c_b)
                  if( p[pixel[8]] < c_b)
                    if( p[pixel[9]] < c_b)
                      if( p[pixel[10]] < c_b)
                        if( p[pixel[11]] < c_b)
                          if( p[pixel[12]] < c_b)
                            if( p[pixel[6]] < c_b)
                              if( p[pixel[5]] < c_b)
                                goto is_a_corner;
                              else
                                if( p[pixel[14]] < c_b)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                            else
                              if( p[pixel[14]] < c_b)
                                if( p[pixel[15]] < c_b)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                if( p[pixel[5]] < c_b)
                  if( p[pixel[6]] < c_b)
                    if( p[pixel[7]] < c_b)
                      if( p[pixel[8]] < c_b)
                        if( p[pixel[9]] < c_b)
                          if( p[pixel[10]] < c_b)
                            if( p[pixel[11]] < c_b)
                              if( p[pixel[12]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
            else
              if( p[pixel[11]] > cb)
                if( p[pixel[12]] > cb)
                  if( p[pixel[13]] > cb)
                    if( p[pixel[14]] > cb)
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              if( p[pixel[9]] > cb)
                                if( p[pixel[10]] > cb)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              if( p[pixel[9]] > cb)
                                if( p[pixel[10]] > cb)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else if( p[pixel[11]] < c_b)
                if( p[pixel[7]] < c_b)
                  if( p[pixel[8]] < c_b)
                    if( p[pixel[9]] < c_b)
                      if( p[pixel[10]] < c_b)
                        if( p[pixel[12]] < c_b)
                          if( p[pixel[13]] < c_b)
                            if( p[pixel[6]] < c_b)
                              if( p[pixel[5]] < c_b)
                                goto is_a_corner;
                              else
                                if( p[pixel[14]] < c_b)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                            else
                              if( p[pixel[14]] < c_b)
                                if( p[pixel[15]] < c_b)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
          else if( p[pixel[3]] < c_b)
            if( p[pixel[10]] > cb)
              if( p[pixel[11]] > cb)
                if( p[pixel[12]] > cb)
                  if( p[pixel[13]] > cb)
                    if( p[pixel[14]] > cb)
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              if( p[pixel[9]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              if( p[pixel[9]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[4]] > cb)
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              if( p[pixel[9]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else if( p[pixel[10]] < c_b)
              if( p[pixel[7]] < c_b)
                if( p[pixel[8]] < c_b)
                  if( p[pixel[9]] < c_b)
                    if( p[pixel[11]] < c_b)
                      if( p[pixel[6]] < c_b)
                        if( p[pixel[5]] < c_b)
                          if( p[pixel[4]] < c_b)
                            goto is_a_corner;
                          else
                            if( p[pixel[12]] < c_b)
                              if( p[pixel[13]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                        else
                          if( p[pixel[12]] < c_b)
                            if( p[pixel[13]] < c_b)
                              if( p[pixel[14]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                      else
                        if( p[pixel[12]] < c_b)
                          if( p[pixel[13]] < c_b)
                            if( p[pixel[14]] < c_b)
                              if( p[pixel[15]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else
            if( p[pixel[10]] > cb)
              if( p[pixel[11]] > cb)
                if( p[pixel[12]] > cb)
                  if( p[pixel[13]] > cb)
                    if( p[pixel[14]] > cb)
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              if( p[pixel[9]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              if( p[pixel[9]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[4]] > cb)
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              if( p[pixel[9]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else if( p[pixel[10]] < c_b)
              if( p[pixel[7]] < c_b)
                if( p[pixel[8]] < c_b)
                  if( p[pixel[9]] < c_b)
                    if( p[pixel[11]] < c_b)
                      if( p[pixel[12]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[5]] < c_b)
                            if( p[pixel[4]] < c_b)
                              goto is_a_corner;
                            else
                              if( p[pixel[13]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                          else
                            if( p[pixel[13]] < c_b)
                              if( p[pixel[14]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                        else
                          if( p[pixel[13]] < c_b)
                            if( p[pixel[14]] < c_b)
                              if( p[pixel[15]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
        else if( p[pixel[2]] < c_b)
          if( p[pixel[9]] > cb)
            if( p[pixel[10]] > cb)
              if( p[pixel[11]] > cb)
                if( p[pixel[12]] > cb)
                  if( p[pixel[13]] > cb)
                    if( p[pixel[14]] > cb)
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[4]] > cb)
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[3]] > cb)
                    if( p[pixel[4]] > cb)
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else if( p[pixel[9]] < c_b)
            if( p[pixel[7]] < c_b)
              if( p[pixel[8]] < c_b)
                if( p[pixel[10]] < c_b)
                  if( p[pixel[6]] < c_b)
                    if( p[pixel[5]] < c_b)
                      if( p[pixel[4]] < c_b)
                        if( p[pixel[3]] < c_b)
                          goto is_a_corner;
                        else
                          if( p[pixel[11]] < c_b)
                            if( p[pixel[12]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                      else
                        if( p[pixel[11]] < c_b)
                          if( p[pixel[12]] < c_b)
                            if( p[pixel[13]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[11]] < c_b)
                        if( p[pixel[12]] < c_b)
                          if( p[pixel[13]] < c_b)
                            if( p[pixel[14]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[11]] < c_b)
                      if( p[pixel[12]] < c_b)
                        if( p[pixel[13]] < c_b)
                          if( p[pixel[14]] < c_b)
                            if( p[pixel[15]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else
            goto is_not_a_corner;
        else
          if( p[pixel[9]] > cb)
            if( p[pixel[10]] > cb)
              if( p[pixel[11]] > cb)
                if( p[pixel[12]] > cb)
                  if( p[pixel[13]] > cb)
                    if( p[pixel[14]] > cb)
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[4]] > cb)
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[3]] > cb)
                    if( p[pixel[4]] > cb)
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            if( p[pixel[8]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else if( p[pixel[9]] < c_b)
            if( p[pixel[7]] < c_b)
              if( p[pixel[8]] < c_b)
                if( p[pixel[10]] < c_b)
                  if( p[pixel[11]] < c_b)
                    if( p[pixel[6]] < c_b)
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[4]] < c_b)
                          if( p[pixel[3]] < c_b)
                            goto is_a_corner;
                          else
                            if( p[pixel[12]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                        else
                          if( p[pixel[12]] < c_b)
                            if( p[pixel[13]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                      else
                        if( p[pixel[12]] < c_b)
                          if( p[pixel[13]] < c_b)
                            if( p[pixel[14]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[12]] < c_b)
                        if( p[pixel[13]] < c_b)
                          if( p[pixel[14]] < c_b)
                            if( p[pixel[15]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else
            goto is_not_a_corner;
      else if( p[pixel[1]] < c_b)
        if( p[pixel[8]] > cb)
          if( p[pixel[9]] > cb)
            if( p[pixel[10]] > cb)
              if( p[pixel[11]] > cb)
                if( p[pixel[12]] > cb)
                  if( p[pixel[13]] > cb)
                    if( p[pixel[14]] > cb)
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[4]] > cb)
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[3]] > cb)
                    if( p[pixel[4]] > cb)
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                if( p[pixel[2]] > cb)
                  if( p[pixel[3]] > cb)
                    if( p[pixel[4]] > cb)
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else
            goto is_not_a_corner;
        else if( p[pixel[8]] < c_b)
          if( p[pixel[7]] < c_b)
            if( p[pixel[9]] < c_b)
              if( p[pixel[6]] < c_b)
                if( p[pixel[5]] < c_b)
                  if( p[pixel[4]] < c_b)
                    if( p[pixel[3]] < c_b)
                      if( p[pixel[2]] < c_b)
                        goto is_a_corner;
                      else
                        if( p[pixel[10]] < c_b)
                          if( p[pixel[11]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[10]] < c_b)
                        if( p[pixel[11]] < c_b)
                          if( p[pixel[12]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[10]] < c_b)
                      if( p[pixel[11]] < c_b)
                        if( p[pixel[12]] < c_b)
                          if( p[pixel[13]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[10]] < c_b)
                    if( p[pixel[11]] < c_b)
                      if( p[pixel[12]] < c_b)
                        if( p[pixel[13]] < c_b)
                          if( p[pixel[14]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                if( p[pixel[10]] < c_b)
                  if( p[pixel[11]] < c_b)
                    if( p[pixel[12]] < c_b)
                      if( p[pixel[13]] < c_b)
                        if( p[pixel[14]] < c_b)
                          if( p[pixel[15]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else
            goto is_not_a_corner;
        else
          goto is_not_a_corner;
      else
        if( p[pixel[8]] > cb)
          if( p[pixel[9]] > cb)
            if( p[pixel[10]] > cb)
              if( p[pixel[11]] > cb)
                if( p[pixel[12]] > cb)
                  if( p[pixel[13]] > cb)
                    if( p[pixel[14]] > cb)
                      if( p[pixel[15]] > cb)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[4]] > cb)
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[3]] > cb)
                    if( p[pixel[4]] > cb)
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                if( p[pixel[2]] > cb)
                  if( p[pixel[3]] > cb)
                    if( p[pixel[4]] > cb)
                      if( p[pixel[5]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[7]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else
            goto is_not_a_corner;
        else if( p[pixel[8]] < c_b)
          if( p[pixel[7]] < c_b)
            if( p[pixel[9]] < c_b)
              if( p[pixel[10]] < c_b)
                if( p[pixel[6]] < c_b)
                  if( p[pixel[5]] < c_b)
                    if( p[pixel[4]] < c_b)
                      if( p[pixel[3]] < c_b)
                        if( p[pixel[2]] < c_b)
                          goto is_a_corner;
                        else
                          if( p[pixel[11]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                      else
                        if( p[pixel[11]] < c_b)
                          if( p[pixel[12]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[11]] < c_b)
                        if( p[pixel[12]] < c_b)
                          if( p[pixel[13]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[11]] < c_b)
                      if( p[pixel[12]] < c_b)
                        if( p[pixel[13]] < c_b)
                          if( p[pixel[14]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[11]] < c_b)
                    if( p[pixel[12]] < c_b)
                      if( p[pixel[13]] < c_b)
                        if( p[pixel[14]] < c_b)
                          if( p[pixel[15]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else
            goto is_not_a_corner;
        else
          goto is_not_a_corner;
    else if( p[pixel[0]] < c_b)
      if( p[pixel[1]] > cb)
        if( p[pixel[8]] > cb)
          if( p[pixel[7]] > cb)
            if( p[pixel[9]] > cb)
              if( p[pixel[6]] > cb)
                if( p[pixel[5]] > cb)
                  if( p[pixel[4]] > cb)
                    if( p[pixel[3]] > cb)
                      if( p[pixel[2]] > cb)
                        goto is_a_corner;
                      else
                        if( p[pixel[10]] > cb)
                          if( p[pixel[11]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[10]] > cb)
                        if( p[pixel[11]] > cb)
                          if( p[pixel[12]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[10]] > cb)
                      if( p[pixel[11]] > cb)
                        if( p[pixel[12]] > cb)
                          if( p[pixel[13]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[10]] > cb)
                    if( p[pixel[11]] > cb)
                      if( p[pixel[12]] > cb)
                        if( p[pixel[13]] > cb)
                          if( p[pixel[14]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                if( p[pixel[10]] > cb)
                  if( p[pixel[11]] > cb)
                    if( p[pixel[12]] > cb)
                      if( p[pixel[13]] > cb)
                        if( p[pixel[14]] > cb)
                          if( p[pixel[15]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else
            goto is_not_a_corner;
        else if( p[pixel[8]] < c_b)
          if( p[pixel[9]] < c_b)
            if( p[pixel[10]] < c_b)
              if( p[pixel[11]] < c_b)
                if( p[pixel[12]] < c_b)
                  if( p[pixel[13]] < c_b)
                    if( p[pixel[14]] < c_b)
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[4]] < c_b)
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[3]] < c_b)
                    if( p[pixel[4]] < c_b)
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                if( p[pixel[2]] < c_b)
                  if( p[pixel[3]] < c_b)
                    if( p[pixel[4]] < c_b)
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else
            goto is_not_a_corner;
        else
          goto is_not_a_corner;
      else if( p[pixel[1]] < c_b)
        if( p[pixel[2]] > cb)
          if( p[pixel[9]] > cb)
            if( p[pixel[7]] > cb)
              if( p[pixel[8]] > cb)
                if( p[pixel[10]] > cb)
                  if( p[pixel[6]] > cb)
                    if( p[pixel[5]] > cb)
                      if( p[pixel[4]] > cb)
                        if( p[pixel[3]] > cb)
                          goto is_a_corner;
                        else
                          if( p[pixel[11]] > cb)
                            if( p[pixel[12]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                      else
                        if( p[pixel[11]] > cb)
                          if( p[pixel[12]] > cb)
                            if( p[pixel[13]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[11]] > cb)
                        if( p[pixel[12]] > cb)
                          if( p[pixel[13]] > cb)
                            if( p[pixel[14]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[11]] > cb)
                      if( p[pixel[12]] > cb)
                        if( p[pixel[13]] > cb)
                          if( p[pixel[14]] > cb)
                            if( p[pixel[15]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else if( p[pixel[9]] < c_b)
            if( p[pixel[10]] < c_b)
              if( p[pixel[11]] < c_b)
                if( p[pixel[12]] < c_b)
                  if( p[pixel[13]] < c_b)
                    if( p[pixel[14]] < c_b)
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[4]] < c_b)
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[3]] < c_b)
                    if( p[pixel[4]] < c_b)
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else
            goto is_not_a_corner;
        else if( p[pixel[2]] < c_b)
          if( p[pixel[3]] > cb)
            if( p[pixel[10]] > cb)
              if( p[pixel[7]] > cb)
                if( p[pixel[8]] > cb)
                  if( p[pixel[9]] > cb)
                    if( p[pixel[11]] > cb)
                      if( p[pixel[6]] > cb)
                        if( p[pixel[5]] > cb)
                          if( p[pixel[4]] > cb)
                            goto is_a_corner;
                          else
                            if( p[pixel[12]] > cb)
                              if( p[pixel[13]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                        else
                          if( p[pixel[12]] > cb)
                            if( p[pixel[13]] > cb)
                              if( p[pixel[14]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                      else
                        if( p[pixel[12]] > cb)
                          if( p[pixel[13]] > cb)
                            if( p[pixel[14]] > cb)
                              if( p[pixel[15]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else if( p[pixel[10]] < c_b)
              if( p[pixel[11]] < c_b)
                if( p[pixel[12]] < c_b)
                  if( p[pixel[13]] < c_b)
                    if( p[pixel[14]] < c_b)
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              if( p[pixel[9]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              if( p[pixel[9]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[4]] < c_b)
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              if( p[pixel[9]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else if( p[pixel[3]] < c_b)
            if( p[pixel[4]] > cb)
              if( p[pixel[13]] > cb)
                if( p[pixel[7]] > cb)
                  if( p[pixel[8]] > cb)
                    if( p[pixel[9]] > cb)
                      if( p[pixel[10]] > cb)
                        if( p[pixel[11]] > cb)
                          if( p[pixel[12]] > cb)
                            if( p[pixel[6]] > cb)
                              if( p[pixel[5]] > cb)
                                goto is_a_corner;
                              else
                                if( p[pixel[14]] > cb)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                            else
                              if( p[pixel[14]] > cb)
                                if( p[pixel[15]] > cb)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else if( p[pixel[13]] < c_b)
                if( p[pixel[11]] > cb)
                  if( p[pixel[5]] > cb)
                    if( p[pixel[6]] > cb)
                      if( p[pixel[7]] > cb)
                        if( p[pixel[8]] > cb)
                          if( p[pixel[9]] > cb)
                            if( p[pixel[10]] > cb)
                              if( p[pixel[12]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else if( p[pixel[11]] < c_b)
                  if( p[pixel[12]] < c_b)
                    if( p[pixel[14]] < c_b)
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              if( p[pixel[9]] < c_b)
                                if( p[pixel[10]] < c_b)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              if( p[pixel[9]] < c_b)
                                if( p[pixel[10]] < c_b)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                if( p[pixel[5]] > cb)
                  if( p[pixel[6]] > cb)
                    if( p[pixel[7]] > cb)
                      if( p[pixel[8]] > cb)
                        if( p[pixel[9]] > cb)
                          if( p[pixel[10]] > cb)
                            if( p[pixel[11]] > cb)
                              if( p[pixel[12]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
            else if( p[pixel[4]] < c_b)
              if( p[pixel[5]] > cb)
                if( p[pixel[14]] > cb)
                  if( p[pixel[7]] > cb)
                    if( p[pixel[8]] > cb)
                      if( p[pixel[9]] > cb)
                        if( p[pixel[10]] > cb)
                          if( p[pixel[11]] > cb)
                            if( p[pixel[12]] > cb)
                              if( p[pixel[13]] > cb)
                                if( p[pixel[6]] > cb)
                                  goto is_a_corner;
                                else
                                  if( p[pixel[15]] > cb)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else if( p[pixel[14]] < c_b)
                  if( p[pixel[12]] > cb)
                    if( p[pixel[6]] > cb)
                      if( p[pixel[7]] > cb)
                        if( p[pixel[8]] > cb)
                          if( p[pixel[9]] > cb)
                            if( p[pixel[10]] > cb)
                              if( p[pixel[11]] > cb)
                                if( p[pixel[13]] > cb)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else if( p[pixel[12]] < c_b)
                    if( p[pixel[13]] < c_b)
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              if( p[pixel[9]] < c_b)
                                if( p[pixel[10]] < c_b)
                                  if( p[pixel[11]] < c_b)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  if( p[pixel[6]] > cb)
                    if( p[pixel[7]] > cb)
                      if( p[pixel[8]] > cb)
                        if( p[pixel[9]] > cb)
                          if( p[pixel[10]] > cb)
                            if( p[pixel[11]] > cb)
                              if( p[pixel[12]] > cb)
                                if( p[pixel[13]] > cb)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else if( p[pixel[5]] < c_b)
                if( p[pixel[6]] > cb)
                  if( p[pixel[15]] < c_b)
                    if( p[pixel[13]] > cb)
                      if( p[pixel[7]] > cb)
                        if( p[pixel[8]] > cb)
                          if( p[pixel[9]] > cb)
                            if( p[pixel[10]] > cb)
                              if( p[pixel[11]] > cb)
                                if( p[pixel[12]] > cb)
                                  if( p[pixel[14]] > cb)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else if( p[pixel[13]] < c_b)
                      if( p[pixel[14]] < c_b)
                        goto is_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    if( p[pixel[7]] > cb)
                      if( p[pixel[8]] > cb)
                        if( p[pixel[9]] > cb)
                          if( p[pixel[10]] > cb)
                            if( p[pixel[11]] > cb)
                              if( p[pixel[12]] > cb)
                                if( p[pixel[13]] > cb)
                                  if( p[pixel[14]] > cb)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else if( p[pixel[6]] < c_b)
                  if( p[pixel[7]] > cb)
                    if( p[pixel[14]] > cb)
                      if( p[pixel[8]] > cb)
                        if( p[pixel[9]] > cb)
                          if( p[pixel[10]] > cb)
                            if( p[pixel[11]] > cb)
                              if( p[pixel[12]] > cb)
                                if( p[pixel[13]] > cb)
                                  if( p[pixel[15]] > cb)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else if( p[pixel[14]] < c_b)
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else if( p[pixel[7]] < c_b)
                    if( p[pixel[8]] < c_b)
                      goto is_a_corner;
                    else
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[14]] < c_b)
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[13]] > cb)
                    if( p[pixel[7]] > cb)
                      if( p[pixel[8]] > cb)
                        if( p[pixel[9]] > cb)
                          if( p[pixel[10]] > cb)
                            if( p[pixel[11]] > cb)
                              if( p[pixel[12]] > cb)
                                if( p[pixel[14]] > cb)
                                  if( p[pixel[15]] > cb)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else if( p[pixel[13]] < c_b)
                    if( p[pixel[14]] < c_b)
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                if( p[pixel[12]] > cb)
                  if( p[pixel[7]] > cb)
                    if( p[pixel[8]] > cb)
                      if( p[pixel[9]] > cb)
                        if( p[pixel[10]] > cb)
                          if( p[pixel[11]] > cb)
                            if( p[pixel[13]] > cb)
                              if( p[pixel[14]] > cb)
                                if( p[pixel[6]] > cb)
                                  goto is_a_corner;
                                else
                                  if( p[pixel[15]] > cb)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else if( p[pixel[12]] < c_b)
                  if( p[pixel[13]] < c_b)
                    if( p[pixel[14]] < c_b)
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              if( p[pixel[9]] < c_b)
                                if( p[pixel[10]] < c_b)
                                  if( p[pixel[11]] < c_b)
                                    goto is_a_corner;
                                  else
                                    goto is_not_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
            else
              if( p[pixel[11]] > cb)
                if( p[pixel[7]] > cb)
                  if( p[pixel[8]] > cb)
                    if( p[pixel[9]] > cb)
                      if( p[pixel[10]] > cb)
                        if( p[pixel[12]] > cb)
                          if( p[pixel[13]] > cb)
                            if( p[pixel[6]] > cb)
                              if( p[pixel[5]] > cb)
                                goto is_a_corner;
                              else
                                if( p[pixel[14]] > cb)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                            else
                              if( p[pixel[14]] > cb)
                                if( p[pixel[15]] > cb)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else if( p[pixel[11]] < c_b)
                if( p[pixel[12]] < c_b)
                  if( p[pixel[13]] < c_b)
                    if( p[pixel[14]] < c_b)
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              if( p[pixel[9]] < c_b)
                                if( p[pixel[10]] < c_b)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              if( p[pixel[9]] < c_b)
                                if( p[pixel[10]] < c_b)
                                  goto is_a_corner;
                                else
                                  goto is_not_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
          else
            if( p[pixel[10]] > cb)
              if( p[pixel[7]] > cb)
                if( p[pixel[8]] > cb)
                  if( p[pixel[9]] > cb)
                    if( p[pixel[11]] > cb)
                      if( p[pixel[12]] > cb)
                        if( p[pixel[6]] > cb)
                          if( p[pixel[5]] > cb)
                            if( p[pixel[4]] > cb)
                              goto is_a_corner;
                            else
                              if( p[pixel[13]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                          else
                            if( p[pixel[13]] > cb)
                              if( p[pixel[14]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                        else
                          if( p[pixel[13]] > cb)
                            if( p[pixel[14]] > cb)
                              if( p[pixel[15]] > cb)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else if( p[pixel[10]] < c_b)
              if( p[pixel[11]] < c_b)
                if( p[pixel[12]] < c_b)
                  if( p[pixel[13]] < c_b)
                    if( p[pixel[14]] < c_b)
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              if( p[pixel[9]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              if( p[pixel[9]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[4]] < c_b)
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              if( p[pixel[9]] < c_b)
                                goto is_a_corner;
                              else
                                goto is_not_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
        else
          if( p[pixel[9]] > cb)
            if( p[pixel[7]] > cb)
              if( p[pixel[8]] > cb)
                if( p[pixel[10]] > cb)
                  if( p[pixel[11]] > cb)
                    if( p[pixel[6]] > cb)
                      if( p[pixel[5]] > cb)
                        if( p[pixel[4]] > cb)
                          if( p[pixel[3]] > cb)
                            goto is_a_corner;
                          else
                            if( p[pixel[12]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                        else
                          if( p[pixel[12]] > cb)
                            if( p[pixel[13]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                      else
                        if( p[pixel[12]] > cb)
                          if( p[pixel[13]] > cb)
                            if( p[pixel[14]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[12]] > cb)
                        if( p[pixel[13]] > cb)
                          if( p[pixel[14]] > cb)
                            if( p[pixel[15]] > cb)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else if( p[pixel[9]] < c_b)
            if( p[pixel[10]] < c_b)
              if( p[pixel[11]] < c_b)
                if( p[pixel[12]] < c_b)
                  if( p[pixel[13]] < c_b)
                    if( p[pixel[14]] < c_b)
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[4]] < c_b)
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[3]] < c_b)
                    if( p[pixel[4]] < c_b)
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            if( p[pixel[8]] < c_b)
                              goto is_a_corner;
                            else
                              goto is_not_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else
            goto is_not_a_corner;
      else
        if( p[pixel[8]] > cb)
          if( p[pixel[7]] > cb)
            if( p[pixel[9]] > cb)
              if( p[pixel[10]] > cb)
                if( p[pixel[6]] > cb)
                  if( p[pixel[5]] > cb)
                    if( p[pixel[4]] > cb)
                      if( p[pixel[3]] > cb)
                        if( p[pixel[2]] > cb)
                          goto is_a_corner;
                        else
                          if( p[pixel[11]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                      else
                        if( p[pixel[11]] > cb)
                          if( p[pixel[12]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[11]] > cb)
                        if( p[pixel[12]] > cb)
                          if( p[pixel[13]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[11]] > cb)
                      if( p[pixel[12]] > cb)
                        if( p[pixel[13]] > cb)
                          if( p[pixel[14]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[11]] > cb)
                    if( p[pixel[12]] > cb)
                      if( p[pixel[13]] > cb)
                        if( p[pixel[14]] > cb)
                          if( p[pixel[15]] > cb)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else
            goto is_not_a_corner;
        else if( p[pixel[8]] < c_b)
          if( p[pixel[9]] < c_b)
            if( p[pixel[10]] < c_b)
              if( p[pixel[11]] < c_b)
                if( p[pixel[12]] < c_b)
                  if( p[pixel[13]] < c_b)
                    if( p[pixel[14]] < c_b)
                      if( p[pixel[15]] < c_b)
                        goto is_a_corner;
                      else
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[4]] < c_b)
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[3]] < c_b)
                    if( p[pixel[4]] < c_b)
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                if( p[pixel[2]] < c_b)
                  if( p[pixel[3]] < c_b)
                    if( p[pixel[4]] < c_b)
                      if( p[pixel[5]] < c_b)
                        if( p[pixel[6]] < c_b)
                          if( p[pixel[7]] < c_b)
                            goto is_a_corner;
                          else
                            goto is_not_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
            else
              goto is_not_a_corner;
          else
            goto is_not_a_corner;
        else
          goto is_not_a_corner;
    else
      if( p[pixel[7]] > cb)
        if( p[pixel[8]] > cb)
          if( p[pixel[9]] > cb)
            if( p[pixel[6]] > cb)
              if( p[pixel[5]] > cb)
                if( p[pixel[4]] > cb)
                  if( p[pixel[3]] > cb)
                    if( p[pixel[2]] > cb)
                      if( p[pixel[1]] > cb)
                        goto is_a_corner;
                      else
                        if( p[pixel[10]] > cb)
                          goto is_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[10]] > cb)
                        if( p[pixel[11]] > cb)
                          goto is_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[10]] > cb)
                      if( p[pixel[11]] > cb)
                        if( p[pixel[12]] > cb)
                          goto is_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[10]] > cb)
                    if( p[pixel[11]] > cb)
                      if( p[pixel[12]] > cb)
                        if( p[pixel[13]] > cb)
                          goto is_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                if( p[pixel[10]] > cb)
                  if( p[pixel[11]] > cb)
                    if( p[pixel[12]] > cb)
                      if( p[pixel[13]] > cb)
                        if( p[pixel[14]] > cb)
                          goto is_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
            else
              if( p[pixel[10]] > cb)
                if( p[pixel[11]] > cb)
                  if( p[pixel[12]] > cb)
                    if( p[pixel[13]] > cb)
                      if( p[pixel[14]] > cb)
                        if( p[pixel[15]] > cb)
                          goto is_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
          else
            goto is_not_a_corner;
        else
          goto is_not_a_corner;
      else if( p[pixel[7]] < c_b)
        if( p[pixel[8]] < c_b)
          if( p[pixel[9]] < c_b)
            if( p[pixel[6]] < c_b)
              if( p[pixel[5]] < c_b)
                if( p[pixel[4]] < c_b)
                  if( p[pixel[3]] < c_b)
                    if( p[pixel[2]] < c_b)
                      if( p[pixel[1]] < c_b)
                        goto is_a_corner;
                      else
                        if( p[pixel[10]] < c_b)
                          goto is_a_corner;
                        else
                          goto is_not_a_corner;
                    else
                      if( p[pixel[10]] < c_b)
                        if( p[pixel[11]] < c_b)
                          goto is_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                  else
                    if( p[pixel[10]] < c_b)
                      if( p[pixel[11]] < c_b)
                        if( p[pixel[12]] < c_b)
                          goto is_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                else
                  if( p[pixel[10]] < c_b)
                    if( p[pixel[11]] < c_b)
                      if( p[pixel[12]] < c_b)
                        if( p[pixel[13]] < c_b)
                          goto is_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
              else
                if( p[pixel[10]] < c_b)
                  if( p[pixel[11]] < c_b)
                    if( p[pixel[12]] < c_b)
                      if( p[pixel[13]] < c_b)
                        if( p[pixel[14]] < c_b)
                          goto is_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
            else
              if( p[pixel[10]] < c_b)
                if( p[pixel[11]] < c_b)
                  if( p[pixel[12]] < c_b)
                    if( p[pixel[13]] < c_b)
                      if( p[pixel[14]] < c_b)
                        if( p[pixel[15]] < c_b)
                          goto is_a_corner;
                        else
                          goto is_not_a_corner;
                      else
                        goto is_not_a_corner;
                    else
                      goto is_not_a_corner;
                  else
                    goto is_not_a_corner;
                else
                  goto is_not_a_corner;
              else
                goto is_not_a_corner;
          else
            goto is_not_a_corner;
        else
          goto is_not_a_corner;
      else
        goto is_not_a_corner;

is_a_corner:
    bmin=b;
    goto end_if;

is_not_a_corner:
    bmax=b;
    goto end_if;

end_if:

    if(bmin == bmax - 1 || bmin == bmax)
      return bmin;
    b = (bmin + bmax) / 2;
  }
}

static void make_offsets(int pixel[], int row_stride)
{
  pixel[0] = 0 + row_stride * 3;
  pixel[1] = 1 + row_stride * 3;
  pixel[2] = 2 + row_stride * 2;
  pixel[3] = 3 + row_stride * 1;
  pixel[4] = 3 + row_stride * 0;
  pixel[5] = 3 + row_stride * -1;
  pixel[6] = 2 + row_stride * -2;
  pixel[7] = 1 + row_stride * -3;
  pixel[8] = 0 + row_stride * -3;
  pixel[9] = -1 + row_stride * -3;
  pixel[10] = -2 + row_stride * -2;
  pixel[11] = -3 + row_stride * -1;
  pixel[12] = -3 + row_stride * 0;
  pixel[13] = -3 + row_stride * 1;
  pixel[14] = -2 + row_stride * 2;
  pixel[15] = -1 + row_stride * 3;
}



int* aom_fast9_score(const byte* i, int stride, const xy* corners, int num_corners, int b)
{
  int* scores = (int*)malloc(sizeof(int)* num_corners);
  int n;

  int pixel[16];
  if(!scores) return NULL;
  make_offsets(pixel, stride);

  for(n=0; n < num_corners; n++)
    scores[n] = fast9_corner_score(i + corners[n].y*stride + corners[n].x, pixel, b);

  return scores;
}


xy* aom_fast9_detect(const byte* im, int xsize, int ysize, int stride, int b, int* ret_num_corners)
{
  int num_corners=0;
  xy* ret_corners;
  int rsize=512;
  int pixel[16];
  int x, y;

  ret_corners = (xy*)malloc(sizeof(xy)*rsize);
  if(!ret_corners) return NULL;
  make_offsets(pixel, stride);

  for(y=3; y < ysize - 3; y++)
    for(x=3; x < xsize - 3; x++)
    {
      const byte* p = im + y*stride + x;

      int cb = *p + b;
      int c_b= *p - b;
      if(p[pixel[0]] > cb)
        if(p[pixel[1]] > cb)
          if(p[pixel[2]] > cb)
            if(p[pixel[3]] > cb)
              if(p[pixel[4]] > cb)
                if(p[pixel[5]] > cb)
                  if(p[pixel[6]] > cb)
                    if(p[pixel[7]] > cb)
                      if(p[pixel[8]] > cb)
                      {}
                      else
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          continue;
                    else if(p[pixel[7]] < c_b)
                      if(p[pixel[14]] > cb)
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          continue;
                      else if(p[pixel[14]] < c_b)
                        if(p[pixel[8]] < c_b)
                          if(p[pixel[9]] < c_b)
                            if(p[pixel[10]] < c_b)
                              if(p[pixel[11]] < c_b)
                                if(p[pixel[12]] < c_b)
                                  if(p[pixel[13]] < c_b)
                                    if(p[pixel[15]] < c_b)
                                    {}
                                    else
                                      continue;
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      if(p[pixel[14]] > cb)
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          continue;
                      else
                        continue;
                  else if(p[pixel[6]] < c_b)
                    if(p[pixel[15]] > cb)
                      if(p[pixel[13]] > cb)
                        if(p[pixel[14]] > cb)
                        {}
                        else
                          continue;
                      else if(p[pixel[13]] < c_b)
                        if(p[pixel[7]] < c_b)
                          if(p[pixel[8]] < c_b)
                            if(p[pixel[9]] < c_b)
                              if(p[pixel[10]] < c_b)
                                if(p[pixel[11]] < c_b)
                                  if(p[pixel[12]] < c_b)
                                    if(p[pixel[14]] < c_b)
                                    {}
                                    else
                                      continue;
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      if(p[pixel[7]] < c_b)
                        if(p[pixel[8]] < c_b)
                          if(p[pixel[9]] < c_b)
                            if(p[pixel[10]] < c_b)
                              if(p[pixel[11]] < c_b)
                                if(p[pixel[12]] < c_b)
                                  if(p[pixel[13]] < c_b)
                                    if(p[pixel[14]] < c_b)
                                    {}
                                    else
                                      continue;
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[13]] > cb)
                      if(p[pixel[14]] > cb)
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          continue;
                      else
                        continue;
                    else if(p[pixel[13]] < c_b)
                      if(p[pixel[7]] < c_b)
                        if(p[pixel[8]] < c_b)
                          if(p[pixel[9]] < c_b)
                            if(p[pixel[10]] < c_b)
                              if(p[pixel[11]] < c_b)
                                if(p[pixel[12]] < c_b)
                                  if(p[pixel[14]] < c_b)
                                    if(p[pixel[15]] < c_b)
                                    {}
                                    else
                                      continue;
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else if(p[pixel[5]] < c_b)
                  if(p[pixel[14]] > cb)
                    if(p[pixel[12]] > cb)
                      if(p[pixel[13]] > cb)
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                                if(p[pixel[9]] > cb)
                                  if(p[pixel[10]] > cb)
                                    if(p[pixel[11]] > cb)
                                    {}
                                    else
                                      continue;
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        continue;
                    else if(p[pixel[12]] < c_b)
                      if(p[pixel[6]] < c_b)
                        if(p[pixel[7]] < c_b)
                          if(p[pixel[8]] < c_b)
                            if(p[pixel[9]] < c_b)
                              if(p[pixel[10]] < c_b)
                                if(p[pixel[11]] < c_b)
                                  if(p[pixel[13]] < c_b)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else if(p[pixel[14]] < c_b)
                    if(p[pixel[7]] < c_b)
                      if(p[pixel[8]] < c_b)
                        if(p[pixel[9]] < c_b)
                          if(p[pixel[10]] < c_b)
                            if(p[pixel[11]] < c_b)
                              if(p[pixel[12]] < c_b)
                                if(p[pixel[13]] < c_b)
                                  if(p[pixel[6]] < c_b)
                                  {}
                                  else
                                    if(p[pixel[15]] < c_b)
                                    {}
                                    else
                                      continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    if(p[pixel[6]] < c_b)
                      if(p[pixel[7]] < c_b)
                        if(p[pixel[8]] < c_b)
                          if(p[pixel[9]] < c_b)
                            if(p[pixel[10]] < c_b)
                              if(p[pixel[11]] < c_b)
                                if(p[pixel[12]] < c_b)
                                  if(p[pixel[13]] < c_b)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  if(p[pixel[12]] > cb)
                    if(p[pixel[13]] > cb)
                      if(p[pixel[14]] > cb)
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                                if(p[pixel[9]] > cb)
                                  if(p[pixel[10]] > cb)
                                    if(p[pixel[11]] > cb)
                                    {}
                                    else
                                      continue;
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        continue;
                    else
                      continue;
                  else if(p[pixel[12]] < c_b)
                    if(p[pixel[7]] < c_b)
                      if(p[pixel[8]] < c_b)
                        if(p[pixel[9]] < c_b)
                          if(p[pixel[10]] < c_b)
                            if(p[pixel[11]] < c_b)
                              if(p[pixel[13]] < c_b)
                                if(p[pixel[14]] < c_b)
                                  if(p[pixel[6]] < c_b)
                                  {}
                                  else
                                    if(p[pixel[15]] < c_b)
                                    {}
                                    else
                                      continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
              else if(p[pixel[4]] < c_b)
                if(p[pixel[13]] > cb)
                  if(p[pixel[11]] > cb)
                    if(p[pixel[12]] > cb)
                      if(p[pixel[14]] > cb)
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                                if(p[pixel[9]] > cb)
                                  if(p[pixel[10]] > cb)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                                if(p[pixel[9]] > cb)
                                  if(p[pixel[10]] > cb)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      continue;
                  else if(p[pixel[11]] < c_b)
                    if(p[pixel[5]] < c_b)
                      if(p[pixel[6]] < c_b)
                        if(p[pixel[7]] < c_b)
                          if(p[pixel[8]] < c_b)
                            if(p[pixel[9]] < c_b)
                              if(p[pixel[10]] < c_b)
                                if(p[pixel[12]] < c_b)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
                else if(p[pixel[13]] < c_b)
                  if(p[pixel[7]] < c_b)
                    if(p[pixel[8]] < c_b)
                      if(p[pixel[9]] < c_b)
                        if(p[pixel[10]] < c_b)
                          if(p[pixel[11]] < c_b)
                            if(p[pixel[12]] < c_b)
                              if(p[pixel[6]] < c_b)
                                if(p[pixel[5]] < c_b)
                                {}
                                else
                                  if(p[pixel[14]] < c_b)
                                  {}
                                  else
                                    continue;
                              else
                                if(p[pixel[14]] < c_b)
                                  if(p[pixel[15]] < c_b)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
                else
                  if(p[pixel[5]] < c_b)
                    if(p[pixel[6]] < c_b)
                      if(p[pixel[7]] < c_b)
                        if(p[pixel[8]] < c_b)
                          if(p[pixel[9]] < c_b)
                            if(p[pixel[10]] < c_b)
                              if(p[pixel[11]] < c_b)
                                if(p[pixel[12]] < c_b)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
              else
                if(p[pixel[11]] > cb)
                  if(p[pixel[12]] > cb)
                    if(p[pixel[13]] > cb)
                      if(p[pixel[14]] > cb)
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                                if(p[pixel[9]] > cb)
                                  if(p[pixel[10]] > cb)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                                if(p[pixel[9]] > cb)
                                  if(p[pixel[10]] > cb)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      continue;
                  else
                    continue;
                else if(p[pixel[11]] < c_b)
                  if(p[pixel[7]] < c_b)
                    if(p[pixel[8]] < c_b)
                      if(p[pixel[9]] < c_b)
                        if(p[pixel[10]] < c_b)
                          if(p[pixel[12]] < c_b)
                            if(p[pixel[13]] < c_b)
                              if(p[pixel[6]] < c_b)
                                if(p[pixel[5]] < c_b)
                                {}
                                else
                                  if(p[pixel[14]] < c_b)
                                  {}
                                  else
                                    continue;
                              else
                                if(p[pixel[14]] < c_b)
                                  if(p[pixel[15]] < c_b)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
                else
                  continue;
            else if(p[pixel[3]] < c_b)
              if(p[pixel[10]] > cb)
                if(p[pixel[11]] > cb)
                  if(p[pixel[12]] > cb)
                    if(p[pixel[13]] > cb)
                      if(p[pixel[14]] > cb)
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                                if(p[pixel[9]] > cb)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                                if(p[pixel[9]] > cb)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[4]] > cb)
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                                if(p[pixel[9]] > cb)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    continue;
                else
                  continue;
              else if(p[pixel[10]] < c_b)
                if(p[pixel[7]] < c_b)
                  if(p[pixel[8]] < c_b)
                    if(p[pixel[9]] < c_b)
                      if(p[pixel[11]] < c_b)
                        if(p[pixel[6]] < c_b)
                          if(p[pixel[5]] < c_b)
                            if(p[pixel[4]] < c_b)
                            {}
                            else
                              if(p[pixel[12]] < c_b)
                                if(p[pixel[13]] < c_b)
                                {}
                                else
                                  continue;
                              else
                                continue;
                          else
                            if(p[pixel[12]] < c_b)
                              if(p[pixel[13]] < c_b)
                                if(p[pixel[14]] < c_b)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                        else
                          if(p[pixel[12]] < c_b)
                            if(p[pixel[13]] < c_b)
                              if(p[pixel[14]] < c_b)
                                if(p[pixel[15]] < c_b)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
                else
                  continue;
              else
                continue;
            else
              if(p[pixel[10]] > cb)
                if(p[pixel[11]] > cb)
                  if(p[pixel[12]] > cb)
                    if(p[pixel[13]] > cb)
                      if(p[pixel[14]] > cb)
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                                if(p[pixel[9]] > cb)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                                if(p[pixel[9]] > cb)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[4]] > cb)
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                                if(p[pixel[9]] > cb)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    continue;
                else
                  continue;
              else if(p[pixel[10]] < c_b)
                if(p[pixel[7]] < c_b)
                  if(p[pixel[8]] < c_b)
                    if(p[pixel[9]] < c_b)
                      if(p[pixel[11]] < c_b)
                        if(p[pixel[12]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[5]] < c_b)
                              if(p[pixel[4]] < c_b)
                              {}
                              else
                                if(p[pixel[13]] < c_b)
                                {}
                                else
                                  continue;
                            else
                              if(p[pixel[13]] < c_b)
                                if(p[pixel[14]] < c_b)
                                {}
                                else
                                  continue;
                              else
                                continue;
                          else
                            if(p[pixel[13]] < c_b)
                              if(p[pixel[14]] < c_b)
                                if(p[pixel[15]] < c_b)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
                else
                  continue;
              else
                continue;
          else if(p[pixel[2]] < c_b)
            if(p[pixel[9]] > cb)
              if(p[pixel[10]] > cb)
                if(p[pixel[11]] > cb)
                  if(p[pixel[12]] > cb)
                    if(p[pixel[13]] > cb)
                      if(p[pixel[14]] > cb)
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[4]] > cb)
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[3]] > cb)
                      if(p[pixel[4]] > cb)
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  continue;
              else
                continue;
            else if(p[pixel[9]] < c_b)
              if(p[pixel[7]] < c_b)
                if(p[pixel[8]] < c_b)
                  if(p[pixel[10]] < c_b)
                    if(p[pixel[6]] < c_b)
                      if(p[pixel[5]] < c_b)
                        if(p[pixel[4]] < c_b)
                          if(p[pixel[3]] < c_b)
                          {}
                          else
                            if(p[pixel[11]] < c_b)
                              if(p[pixel[12]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                        else
                          if(p[pixel[11]] < c_b)
                            if(p[pixel[12]] < c_b)
                              if(p[pixel[13]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[11]] < c_b)
                          if(p[pixel[12]] < c_b)
                            if(p[pixel[13]] < c_b)
                              if(p[pixel[14]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[11]] < c_b)
                        if(p[pixel[12]] < c_b)
                          if(p[pixel[13]] < c_b)
                            if(p[pixel[14]] < c_b)
                              if(p[pixel[15]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    continue;
                else
                  continue;
              else
                continue;
            else
              continue;
          else
            if(p[pixel[9]] > cb)
              if(p[pixel[10]] > cb)
                if(p[pixel[11]] > cb)
                  if(p[pixel[12]] > cb)
                    if(p[pixel[13]] > cb)
                      if(p[pixel[14]] > cb)
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[4]] > cb)
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[3]] > cb)
                      if(p[pixel[4]] > cb)
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                              if(p[pixel[8]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  continue;
              else
                continue;
            else if(p[pixel[9]] < c_b)
              if(p[pixel[7]] < c_b)
                if(p[pixel[8]] < c_b)
                  if(p[pixel[10]] < c_b)
                    if(p[pixel[11]] < c_b)
                      if(p[pixel[6]] < c_b)
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[4]] < c_b)
                            if(p[pixel[3]] < c_b)
                            {}
                            else
                              if(p[pixel[12]] < c_b)
                              {}
                              else
                                continue;
                          else
                            if(p[pixel[12]] < c_b)
                              if(p[pixel[13]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                        else
                          if(p[pixel[12]] < c_b)
                            if(p[pixel[13]] < c_b)
                              if(p[pixel[14]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[12]] < c_b)
                          if(p[pixel[13]] < c_b)
                            if(p[pixel[14]] < c_b)
                              if(p[pixel[15]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      continue;
                  else
                    continue;
                else
                  continue;
              else
                continue;
            else
              continue;
        else if(p[pixel[1]] < c_b)
          if(p[pixel[8]] > cb)
            if(p[pixel[9]] > cb)
              if(p[pixel[10]] > cb)
                if(p[pixel[11]] > cb)
                  if(p[pixel[12]] > cb)
                    if(p[pixel[13]] > cb)
                      if(p[pixel[14]] > cb)
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[4]] > cb)
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[3]] > cb)
                      if(p[pixel[4]] > cb)
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  if(p[pixel[2]] > cb)
                    if(p[pixel[3]] > cb)
                      if(p[pixel[4]] > cb)
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
              else
                continue;
            else
              continue;
          else if(p[pixel[8]] < c_b)
            if(p[pixel[7]] < c_b)
              if(p[pixel[9]] < c_b)
                if(p[pixel[6]] < c_b)
                  if(p[pixel[5]] < c_b)
                    if(p[pixel[4]] < c_b)
                      if(p[pixel[3]] < c_b)
                        if(p[pixel[2]] < c_b)
                        {}
                        else
                          if(p[pixel[10]] < c_b)
                            if(p[pixel[11]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[10]] < c_b)
                          if(p[pixel[11]] < c_b)
                            if(p[pixel[12]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[10]] < c_b)
                        if(p[pixel[11]] < c_b)
                          if(p[pixel[12]] < c_b)
                            if(p[pixel[13]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[10]] < c_b)
                      if(p[pixel[11]] < c_b)
                        if(p[pixel[12]] < c_b)
                          if(p[pixel[13]] < c_b)
                            if(p[pixel[14]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  if(p[pixel[10]] < c_b)
                    if(p[pixel[11]] < c_b)
                      if(p[pixel[12]] < c_b)
                        if(p[pixel[13]] < c_b)
                          if(p[pixel[14]] < c_b)
                            if(p[pixel[15]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
              else
                continue;
            else
              continue;
          else
            continue;
        else
          if(p[pixel[8]] > cb)
            if(p[pixel[9]] > cb)
              if(p[pixel[10]] > cb)
                if(p[pixel[11]] > cb)
                  if(p[pixel[12]] > cb)
                    if(p[pixel[13]] > cb)
                      if(p[pixel[14]] > cb)
                        if(p[pixel[15]] > cb)
                        {}
                        else
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[4]] > cb)
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[3]] > cb)
                      if(p[pixel[4]] > cb)
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  if(p[pixel[2]] > cb)
                    if(p[pixel[3]] > cb)
                      if(p[pixel[4]] > cb)
                        if(p[pixel[5]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[7]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
              else
                continue;
            else
              continue;
          else if(p[pixel[8]] < c_b)
            if(p[pixel[7]] < c_b)
              if(p[pixel[9]] < c_b)
                if(p[pixel[10]] < c_b)
                  if(p[pixel[6]] < c_b)
                    if(p[pixel[5]] < c_b)
                      if(p[pixel[4]] < c_b)
                        if(p[pixel[3]] < c_b)
                          if(p[pixel[2]] < c_b)
                          {}
                          else
                            if(p[pixel[11]] < c_b)
                            {}
                            else
                              continue;
                        else
                          if(p[pixel[11]] < c_b)
                            if(p[pixel[12]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[11]] < c_b)
                          if(p[pixel[12]] < c_b)
                            if(p[pixel[13]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[11]] < c_b)
                        if(p[pixel[12]] < c_b)
                          if(p[pixel[13]] < c_b)
                            if(p[pixel[14]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[11]] < c_b)
                      if(p[pixel[12]] < c_b)
                        if(p[pixel[13]] < c_b)
                          if(p[pixel[14]] < c_b)
                            if(p[pixel[15]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  continue;
              else
                continue;
            else
              continue;
          else
            continue;
      else if(p[pixel[0]] < c_b)
        if(p[pixel[1]] > cb)
          if(p[pixel[8]] > cb)
            if(p[pixel[7]] > cb)
              if(p[pixel[9]] > cb)
                if(p[pixel[6]] > cb)
                  if(p[pixel[5]] > cb)
                    if(p[pixel[4]] > cb)
                      if(p[pixel[3]] > cb)
                        if(p[pixel[2]] > cb)
                        {}
                        else
                          if(p[pixel[10]] > cb)
                            if(p[pixel[11]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[10]] > cb)
                          if(p[pixel[11]] > cb)
                            if(p[pixel[12]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[10]] > cb)
                        if(p[pixel[11]] > cb)
                          if(p[pixel[12]] > cb)
                            if(p[pixel[13]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[10]] > cb)
                      if(p[pixel[11]] > cb)
                        if(p[pixel[12]] > cb)
                          if(p[pixel[13]] > cb)
                            if(p[pixel[14]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  if(p[pixel[10]] > cb)
                    if(p[pixel[11]] > cb)
                      if(p[pixel[12]] > cb)
                        if(p[pixel[13]] > cb)
                          if(p[pixel[14]] > cb)
                            if(p[pixel[15]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
              else
                continue;
            else
              continue;
          else if(p[pixel[8]] < c_b)
            if(p[pixel[9]] < c_b)
              if(p[pixel[10]] < c_b)
                if(p[pixel[11]] < c_b)
                  if(p[pixel[12]] < c_b)
                    if(p[pixel[13]] < c_b)
                      if(p[pixel[14]] < c_b)
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[4]] < c_b)
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[3]] < c_b)
                      if(p[pixel[4]] < c_b)
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  if(p[pixel[2]] < c_b)
                    if(p[pixel[3]] < c_b)
                      if(p[pixel[4]] < c_b)
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
              else
                continue;
            else
              continue;
          else
            continue;
        else if(p[pixel[1]] < c_b)
          if(p[pixel[2]] > cb)
            if(p[pixel[9]] > cb)
              if(p[pixel[7]] > cb)
                if(p[pixel[8]] > cb)
                  if(p[pixel[10]] > cb)
                    if(p[pixel[6]] > cb)
                      if(p[pixel[5]] > cb)
                        if(p[pixel[4]] > cb)
                          if(p[pixel[3]] > cb)
                          {}
                          else
                            if(p[pixel[11]] > cb)
                              if(p[pixel[12]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                        else
                          if(p[pixel[11]] > cb)
                            if(p[pixel[12]] > cb)
                              if(p[pixel[13]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[11]] > cb)
                          if(p[pixel[12]] > cb)
                            if(p[pixel[13]] > cb)
                              if(p[pixel[14]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[11]] > cb)
                        if(p[pixel[12]] > cb)
                          if(p[pixel[13]] > cb)
                            if(p[pixel[14]] > cb)
                              if(p[pixel[15]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    continue;
                else
                  continue;
              else
                continue;
            else if(p[pixel[9]] < c_b)
              if(p[pixel[10]] < c_b)
                if(p[pixel[11]] < c_b)
                  if(p[pixel[12]] < c_b)
                    if(p[pixel[13]] < c_b)
                      if(p[pixel[14]] < c_b)
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[4]] < c_b)
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[3]] < c_b)
                      if(p[pixel[4]] < c_b)
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  continue;
              else
                continue;
            else
              continue;
          else if(p[pixel[2]] < c_b)
            if(p[pixel[3]] > cb)
              if(p[pixel[10]] > cb)
                if(p[pixel[7]] > cb)
                  if(p[pixel[8]] > cb)
                    if(p[pixel[9]] > cb)
                      if(p[pixel[11]] > cb)
                        if(p[pixel[6]] > cb)
                          if(p[pixel[5]] > cb)
                            if(p[pixel[4]] > cb)
                            {}
                            else
                              if(p[pixel[12]] > cb)
                                if(p[pixel[13]] > cb)
                                {}
                                else
                                  continue;
                              else
                                continue;
                          else
                            if(p[pixel[12]] > cb)
                              if(p[pixel[13]] > cb)
                                if(p[pixel[14]] > cb)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                        else
                          if(p[pixel[12]] > cb)
                            if(p[pixel[13]] > cb)
                              if(p[pixel[14]] > cb)
                                if(p[pixel[15]] > cb)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
                else
                  continue;
              else if(p[pixel[10]] < c_b)
                if(p[pixel[11]] < c_b)
                  if(p[pixel[12]] < c_b)
                    if(p[pixel[13]] < c_b)
                      if(p[pixel[14]] < c_b)
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                                if(p[pixel[9]] < c_b)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                                if(p[pixel[9]] < c_b)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[4]] < c_b)
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                                if(p[pixel[9]] < c_b)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    continue;
                else
                  continue;
              else
                continue;
            else if(p[pixel[3]] < c_b)
              if(p[pixel[4]] > cb)
                if(p[pixel[13]] > cb)
                  if(p[pixel[7]] > cb)
                    if(p[pixel[8]] > cb)
                      if(p[pixel[9]] > cb)
                        if(p[pixel[10]] > cb)
                          if(p[pixel[11]] > cb)
                            if(p[pixel[12]] > cb)
                              if(p[pixel[6]] > cb)
                                if(p[pixel[5]] > cb)
                                {}
                                else
                                  if(p[pixel[14]] > cb)
                                  {}
                                  else
                                    continue;
                              else
                                if(p[pixel[14]] > cb)
                                  if(p[pixel[15]] > cb)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
                else if(p[pixel[13]] < c_b)
                  if(p[pixel[11]] > cb)
                    if(p[pixel[5]] > cb)
                      if(p[pixel[6]] > cb)
                        if(p[pixel[7]] > cb)
                          if(p[pixel[8]] > cb)
                            if(p[pixel[9]] > cb)
                              if(p[pixel[10]] > cb)
                                if(p[pixel[12]] > cb)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else if(p[pixel[11]] < c_b)
                    if(p[pixel[12]] < c_b)
                      if(p[pixel[14]] < c_b)
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                                if(p[pixel[9]] < c_b)
                                  if(p[pixel[10]] < c_b)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                                if(p[pixel[9]] < c_b)
                                  if(p[pixel[10]] < c_b)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      continue;
                  else
                    continue;
                else
                  if(p[pixel[5]] > cb)
                    if(p[pixel[6]] > cb)
                      if(p[pixel[7]] > cb)
                        if(p[pixel[8]] > cb)
                          if(p[pixel[9]] > cb)
                            if(p[pixel[10]] > cb)
                              if(p[pixel[11]] > cb)
                                if(p[pixel[12]] > cb)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
              else if(p[pixel[4]] < c_b)
                if(p[pixel[5]] > cb)
                  if(p[pixel[14]] > cb)
                    if(p[pixel[7]] > cb)
                      if(p[pixel[8]] > cb)
                        if(p[pixel[9]] > cb)
                          if(p[pixel[10]] > cb)
                            if(p[pixel[11]] > cb)
                              if(p[pixel[12]] > cb)
                                if(p[pixel[13]] > cb)
                                  if(p[pixel[6]] > cb)
                                  {}
                                  else
                                    if(p[pixel[15]] > cb)
                                    {}
                                    else
                                      continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else if(p[pixel[14]] < c_b)
                    if(p[pixel[12]] > cb)
                      if(p[pixel[6]] > cb)
                        if(p[pixel[7]] > cb)
                          if(p[pixel[8]] > cb)
                            if(p[pixel[9]] > cb)
                              if(p[pixel[10]] > cb)
                                if(p[pixel[11]] > cb)
                                  if(p[pixel[13]] > cb)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else if(p[pixel[12]] < c_b)
                      if(p[pixel[13]] < c_b)
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                                if(p[pixel[9]] < c_b)
                                  if(p[pixel[10]] < c_b)
                                    if(p[pixel[11]] < c_b)
                                    {}
                                    else
                                      continue;
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    if(p[pixel[6]] > cb)
                      if(p[pixel[7]] > cb)
                        if(p[pixel[8]] > cb)
                          if(p[pixel[9]] > cb)
                            if(p[pixel[10]] > cb)
                              if(p[pixel[11]] > cb)
                                if(p[pixel[12]] > cb)
                                  if(p[pixel[13]] > cb)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else if(p[pixel[5]] < c_b)
                  if(p[pixel[6]] > cb)
                    if(p[pixel[15]] < c_b)
                      if(p[pixel[13]] > cb)
                        if(p[pixel[7]] > cb)
                          if(p[pixel[8]] > cb)
                            if(p[pixel[9]] > cb)
                              if(p[pixel[10]] > cb)
                                if(p[pixel[11]] > cb)
                                  if(p[pixel[12]] > cb)
                                    if(p[pixel[14]] > cb)
                                    {}
                                    else
                                      continue;
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else if(p[pixel[13]] < c_b)
                        if(p[pixel[14]] < c_b)
                        {}
                        else
                          continue;
                      else
                        continue;
                    else
                      if(p[pixel[7]] > cb)
                        if(p[pixel[8]] > cb)
                          if(p[pixel[9]] > cb)
                            if(p[pixel[10]] > cb)
                              if(p[pixel[11]] > cb)
                                if(p[pixel[12]] > cb)
                                  if(p[pixel[13]] > cb)
                                    if(p[pixel[14]] > cb)
                                    {}
                                    else
                                      continue;
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else if(p[pixel[6]] < c_b)
                    if(p[pixel[7]] > cb)
                      if(p[pixel[14]] > cb)
                        if(p[pixel[8]] > cb)
                          if(p[pixel[9]] > cb)
                            if(p[pixel[10]] > cb)
                              if(p[pixel[11]] > cb)
                                if(p[pixel[12]] > cb)
                                  if(p[pixel[13]] > cb)
                                    if(p[pixel[15]] > cb)
                                    {}
                                    else
                                      continue;
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else if(p[pixel[14]] < c_b)
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          continue;
                      else
                        continue;
                    else if(p[pixel[7]] < c_b)
                      if(p[pixel[8]] < c_b)
                      {}
                      else
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          continue;
                    else
                      if(p[pixel[14]] < c_b)
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[13]] > cb)
                      if(p[pixel[7]] > cb)
                        if(p[pixel[8]] > cb)
                          if(p[pixel[9]] > cb)
                            if(p[pixel[10]] > cb)
                              if(p[pixel[11]] > cb)
                                if(p[pixel[12]] > cb)
                                  if(p[pixel[14]] > cb)
                                    if(p[pixel[15]] > cb)
                                    {}
                                    else
                                      continue;
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else if(p[pixel[13]] < c_b)
                      if(p[pixel[14]] < c_b)
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  if(p[pixel[12]] > cb)
                    if(p[pixel[7]] > cb)
                      if(p[pixel[8]] > cb)
                        if(p[pixel[9]] > cb)
                          if(p[pixel[10]] > cb)
                            if(p[pixel[11]] > cb)
                              if(p[pixel[13]] > cb)
                                if(p[pixel[14]] > cb)
                                  if(p[pixel[6]] > cb)
                                  {}
                                  else
                                    if(p[pixel[15]] > cb)
                                    {}
                                    else
                                      continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else if(p[pixel[12]] < c_b)
                    if(p[pixel[13]] < c_b)
                      if(p[pixel[14]] < c_b)
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                                if(p[pixel[9]] < c_b)
                                  if(p[pixel[10]] < c_b)
                                    if(p[pixel[11]] < c_b)
                                    {}
                                    else
                                      continue;
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
              else
                if(p[pixel[11]] > cb)
                  if(p[pixel[7]] > cb)
                    if(p[pixel[8]] > cb)
                      if(p[pixel[9]] > cb)
                        if(p[pixel[10]] > cb)
                          if(p[pixel[12]] > cb)
                            if(p[pixel[13]] > cb)
                              if(p[pixel[6]] > cb)
                                if(p[pixel[5]] > cb)
                                {}
                                else
                                  if(p[pixel[14]] > cb)
                                  {}
                                  else
                                    continue;
                              else
                                if(p[pixel[14]] > cb)
                                  if(p[pixel[15]] > cb)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
                else if(p[pixel[11]] < c_b)
                  if(p[pixel[12]] < c_b)
                    if(p[pixel[13]] < c_b)
                      if(p[pixel[14]] < c_b)
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                                if(p[pixel[9]] < c_b)
                                  if(p[pixel[10]] < c_b)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                                if(p[pixel[9]] < c_b)
                                  if(p[pixel[10]] < c_b)
                                  {}
                                  else
                                    continue;
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      continue;
                  else
                    continue;
                else
                  continue;
            else
              if(p[pixel[10]] > cb)
                if(p[pixel[7]] > cb)
                  if(p[pixel[8]] > cb)
                    if(p[pixel[9]] > cb)
                      if(p[pixel[11]] > cb)
                        if(p[pixel[12]] > cb)
                          if(p[pixel[6]] > cb)
                            if(p[pixel[5]] > cb)
                              if(p[pixel[4]] > cb)
                              {}
                              else
                                if(p[pixel[13]] > cb)
                                {}
                                else
                                  continue;
                            else
                              if(p[pixel[13]] > cb)
                                if(p[pixel[14]] > cb)
                                {}
                                else
                                  continue;
                              else
                                continue;
                          else
                            if(p[pixel[13]] > cb)
                              if(p[pixel[14]] > cb)
                                if(p[pixel[15]] > cb)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
                else
                  continue;
              else if(p[pixel[10]] < c_b)
                if(p[pixel[11]] < c_b)
                  if(p[pixel[12]] < c_b)
                    if(p[pixel[13]] < c_b)
                      if(p[pixel[14]] < c_b)
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                                if(p[pixel[9]] < c_b)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                                if(p[pixel[9]] < c_b)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[4]] < c_b)
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                                if(p[pixel[9]] < c_b)
                                {}
                                else
                                  continue;
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    continue;
                else
                  continue;
              else
                continue;
          else
            if(p[pixel[9]] > cb)
              if(p[pixel[7]] > cb)
                if(p[pixel[8]] > cb)
                  if(p[pixel[10]] > cb)
                    if(p[pixel[11]] > cb)
                      if(p[pixel[6]] > cb)
                        if(p[pixel[5]] > cb)
                          if(p[pixel[4]] > cb)
                            if(p[pixel[3]] > cb)
                            {}
                            else
                              if(p[pixel[12]] > cb)
                              {}
                              else
                                continue;
                          else
                            if(p[pixel[12]] > cb)
                              if(p[pixel[13]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                        else
                          if(p[pixel[12]] > cb)
                            if(p[pixel[13]] > cb)
                              if(p[pixel[14]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[12]] > cb)
                          if(p[pixel[13]] > cb)
                            if(p[pixel[14]] > cb)
                              if(p[pixel[15]] > cb)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      continue;
                  else
                    continue;
                else
                  continue;
              else
                continue;
            else if(p[pixel[9]] < c_b)
              if(p[pixel[10]] < c_b)
                if(p[pixel[11]] < c_b)
                  if(p[pixel[12]] < c_b)
                    if(p[pixel[13]] < c_b)
                      if(p[pixel[14]] < c_b)
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[4]] < c_b)
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[3]] < c_b)
                      if(p[pixel[4]] < c_b)
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                              if(p[pixel[8]] < c_b)
                              {}
                              else
                                continue;
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  continue;
              else
                continue;
            else
              continue;
        else
          if(p[pixel[8]] > cb)
            if(p[pixel[7]] > cb)
              if(p[pixel[9]] > cb)
                if(p[pixel[10]] > cb)
                  if(p[pixel[6]] > cb)
                    if(p[pixel[5]] > cb)
                      if(p[pixel[4]] > cb)
                        if(p[pixel[3]] > cb)
                          if(p[pixel[2]] > cb)
                          {}
                          else
                            if(p[pixel[11]] > cb)
                            {}
                            else
                              continue;
                        else
                          if(p[pixel[11]] > cb)
                            if(p[pixel[12]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[11]] > cb)
                          if(p[pixel[12]] > cb)
                            if(p[pixel[13]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[11]] > cb)
                        if(p[pixel[12]] > cb)
                          if(p[pixel[13]] > cb)
                            if(p[pixel[14]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[11]] > cb)
                      if(p[pixel[12]] > cb)
                        if(p[pixel[13]] > cb)
                          if(p[pixel[14]] > cb)
                            if(p[pixel[15]] > cb)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  continue;
              else
                continue;
            else
              continue;
          else if(p[pixel[8]] < c_b)
            if(p[pixel[9]] < c_b)
              if(p[pixel[10]] < c_b)
                if(p[pixel[11]] < c_b)
                  if(p[pixel[12]] < c_b)
                    if(p[pixel[13]] < c_b)
                      if(p[pixel[14]] < c_b)
                        if(p[pixel[15]] < c_b)
                        {}
                        else
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                      else
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[4]] < c_b)
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[3]] < c_b)
                      if(p[pixel[4]] < c_b)
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  if(p[pixel[2]] < c_b)
                    if(p[pixel[3]] < c_b)
                      if(p[pixel[4]] < c_b)
                        if(p[pixel[5]] < c_b)
                          if(p[pixel[6]] < c_b)
                            if(p[pixel[7]] < c_b)
                            {}
                            else
                              continue;
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
              else
                continue;
            else
              continue;
          else
            continue;
      else
        if(p[pixel[7]] > cb)
          if(p[pixel[8]] > cb)
            if(p[pixel[9]] > cb)
              if(p[pixel[6]] > cb)
                if(p[pixel[5]] > cb)
                  if(p[pixel[4]] > cb)
                    if(p[pixel[3]] > cb)
                      if(p[pixel[2]] > cb)
                        if(p[pixel[1]] > cb)
                        {}
                        else
                          if(p[pixel[10]] > cb)
                          {}
                          else
                            continue;
                      else
                        if(p[pixel[10]] > cb)
                          if(p[pixel[11]] > cb)
                          {}
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[10]] > cb)
                        if(p[pixel[11]] > cb)
                          if(p[pixel[12]] > cb)
                          {}
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[10]] > cb)
                      if(p[pixel[11]] > cb)
                        if(p[pixel[12]] > cb)
                          if(p[pixel[13]] > cb)
                          {}
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  if(p[pixel[10]] > cb)
                    if(p[pixel[11]] > cb)
                      if(p[pixel[12]] > cb)
                        if(p[pixel[13]] > cb)
                          if(p[pixel[14]] > cb)
                          {}
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
              else
                if(p[pixel[10]] > cb)
                  if(p[pixel[11]] > cb)
                    if(p[pixel[12]] > cb)
                      if(p[pixel[13]] > cb)
                        if(p[pixel[14]] > cb)
                          if(p[pixel[15]] > cb)
                          {}
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
                else
                  continue;
            else
              continue;
          else
            continue;
        else if(p[pixel[7]] < c_b)
          if(p[pixel[8]] < c_b)
            if(p[pixel[9]] < c_b)
              if(p[pixel[6]] < c_b)
                if(p[pixel[5]] < c_b)
                  if(p[pixel[4]] < c_b)
                    if(p[pixel[3]] < c_b)
                      if(p[pixel[2]] < c_b)
                        if(p[pixel[1]] < c_b)
                        {}
                        else
                          if(p[pixel[10]] < c_b)
                          {}
                          else
                            continue;
                      else
                        if(p[pixel[10]] < c_b)
                          if(p[pixel[11]] < c_b)
                          {}
                          else
                            continue;
                        else
                          continue;
                    else
                      if(p[pixel[10]] < c_b)
                        if(p[pixel[11]] < c_b)
                          if(p[pixel[12]] < c_b)
                          {}
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                  else
                    if(p[pixel[10]] < c_b)
                      if(p[pixel[11]] < c_b)
                        if(p[pixel[12]] < c_b)
                          if(p[pixel[13]] < c_b)
                          {}
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                else
                  if(p[pixel[10]] < c_b)
                    if(p[pixel[11]] < c_b)
                      if(p[pixel[12]] < c_b)
                        if(p[pixel[13]] < c_b)
                          if(p[pixel[14]] < c_b)
                          {}
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
              else
                if(p[pixel[10]] < c_b)
                  if(p[pixel[11]] < c_b)
                    if(p[pixel[12]] < c_b)
                      if(p[pixel[13]] < c_b)
                        if(p[pixel[14]] < c_b)
                          if(p[pixel[15]] < c_b)
                          {}
                          else
                            continue;
                        else
                          continue;
                      else
                        continue;
                    else
                      continue;
                  else
                    continue;
                else
                  continue;
            else
              continue;
          else
            continue;
        else
          continue;
      if(num_corners == rsize)
      {
        rsize*=2;
        xy* new_ret_corners = (xy*)realloc(ret_corners, sizeof(xy)*rsize);
        if(!new_ret_corners)
        {
          free(ret_corners);
          return NULL;
        }
        ret_corners = new_ret_corners;
      }
      ret_corners[num_corners].x = x;
      ret_corners[num_corners].y = y;
      num_corners++;

    }

  *ret_num_corners = num_corners;
  return ret_corners;

}

// clang-format on
