/* Copyright (c) 2008-2011 Octasic Inc.
   Written by Jean-Marc Valin */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _MLP_TRAIN_H_
#define _MLP_TRAIN_H_

#include <math.h>
#include <stdlib.h>

double tansig_table[501];
static inline double tansig_double(double x)
{
    return 2./(1.+exp(-2.*x)) - 1.;
}
static inline void build_tansig_table(void)
{
    int i;
    for (i=0;i<501;i++)
        tansig_table[i] = tansig_double(.04*(i-250));
}

static inline double tansig_approx(double x)
{
    int i;
    double y, dy;
    if (x>=10)
        return 1;
    if (x<=-10)
        return -1;
    i = lrint(25*x);
    x -= .04*i;
    y = tansig_table[250+i];
    dy = 1-y*y;
    y = y + x*dy*(1 - y*x);
    return y;
}

static inline float randn(float sd)
{
   float U1, U2, S, x;
   do {
      U1 = ((float)rand())/RAND_MAX;
      U2 = ((float)rand())/RAND_MAX;
      U1 = 2*U1-1;
      U2 = 2*U2-1;
      S = U1*U1 + U2*U2;
   } while (S >= 1 || S == 0.0f);
   x = sd*sqrt(-2 * log(S) / S) * U1;
   return x;
}


typedef struct {
    int layers;
    int *topo;
    double **weights;
    double **best_weights;
    double *in_rate;
} MLPTrain;


#endif /* _MLP_TRAIN_H_ */
