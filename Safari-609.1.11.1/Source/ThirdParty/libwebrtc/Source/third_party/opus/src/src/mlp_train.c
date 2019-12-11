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


#include "mlp_train.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>

int stopped = 0;

void handler(int sig)
{
    stopped = 1;
    signal(sig, handler);
}

MLPTrain * mlp_init(int *topo, int nbLayers, float *inputs, float *outputs, int nbSamples)
{
    int i, j, k;
    MLPTrain *net;
    int inDim, outDim;
    net = malloc(sizeof(*net));
    net->topo = malloc(nbLayers*sizeof(net->topo[0]));
    for (i=0;i<nbLayers;i++)
        net->topo[i] = topo[i];
    inDim = topo[0];
    outDim = topo[nbLayers-1];
    net->in_rate = malloc((inDim+1)*sizeof(net->in_rate[0]));
    net->weights = malloc((nbLayers-1)*sizeof(net->weights));
    net->best_weights = malloc((nbLayers-1)*sizeof(net->weights));
    for (i=0;i<nbLayers-1;i++)
    {
        net->weights[i] = malloc((topo[i]+1)*topo[i+1]*sizeof(net->weights[0][0]));
        net->best_weights[i] = malloc((topo[i]+1)*topo[i+1]*sizeof(net->weights[0][0]));
    }
    double inMean[inDim];
    for (j=0;j<inDim;j++)
    {
        double std=0;
        inMean[j] = 0;
        for (i=0;i<nbSamples;i++)
        {
            inMean[j] += inputs[i*inDim+j];
            std += inputs[i*inDim+j]*inputs[i*inDim+j];
        }
        inMean[j] /= nbSamples;
        std /= nbSamples;
        net->in_rate[1+j] = .5/(.0001+std);
        std = std-inMean[j]*inMean[j];
        if (std<.001)
            std = .001;
        std = 1/sqrt(inDim*std);
        for (k=0;k<topo[1];k++)
            net->weights[0][k*(topo[0]+1)+j+1] = randn(std);
    }
    net->in_rate[0] = 1;
    for (j=0;j<topo[1];j++)
    {
        double sum = 0;
        for (k=0;k<inDim;k++)
            sum += inMean[k]*net->weights[0][j*(topo[0]+1)+k+1];
        net->weights[0][j*(topo[0]+1)] = -sum;
    }
    for (j=0;j<outDim;j++)
    {
        double mean = 0;
        double std;
        for (i=0;i<nbSamples;i++)
            mean += outputs[i*outDim+j];
        mean /= nbSamples;
        std = 1/sqrt(topo[nbLayers-2]);
        net->weights[nbLayers-2][j*(topo[nbLayers-2]+1)] = mean;
        for (k=0;k<topo[nbLayers-2];k++)
            net->weights[nbLayers-2][j*(topo[nbLayers-2]+1)+k+1] = randn(std);
    }
    return net;
}

#define MAX_NEURONS 100
#define MAX_OUT 10

double compute_gradient(MLPTrain *net, float *inputs, float *outputs, int nbSamples, double *W0_grad, double *W1_grad, double *error_rate)
{
    int i,j;
    int s;
    int inDim, outDim, hiddenDim;
    int *topo;
    double *W0, *W1;
    double rms=0;
    int W0_size, W1_size;
    double hidden[MAX_NEURONS];
    double netOut[MAX_NEURONS];
    double error[MAX_NEURONS];

    topo = net->topo;
    inDim = net->topo[0];
    hiddenDim = net->topo[1];
    outDim = net->topo[2];
    W0_size = (topo[0]+1)*topo[1];
    W1_size = (topo[1]+1)*topo[2];
    W0 = net->weights[0];
    W1 = net->weights[1];
    memset(W0_grad, 0, W0_size*sizeof(double));
    memset(W1_grad, 0, W1_size*sizeof(double));
    for (i=0;i<outDim;i++)
        netOut[i] = outputs[i];
    for (i=0;i<outDim;i++)
        error_rate[i] = 0;
    for (s=0;s<nbSamples;s++)
    {
        float *in, *out;
        float inp[inDim];
        in = inputs+s*inDim;
        out = outputs + s*outDim;
        for (j=0;j<inDim;j++)
           inp[j] = in[j];
        for (i=0;i<hiddenDim;i++)
        {
            double sum = W0[i*(inDim+1)];
            for (j=0;j<inDim;j++)
                sum += W0[i*(inDim+1)+j+1]*inp[j];
            hidden[i] = tansig_approx(sum);
        }
        for (i=0;i<outDim;i++)
        {
            double sum = W1[i*(hiddenDim+1)];
            for (j=0;j<hiddenDim;j++)
                sum += W1[i*(hiddenDim+1)+j+1]*hidden[j];
            netOut[i] = tansig_approx(sum);
            error[i] = out[i] - netOut[i];
            if (out[i] == 0) error[i] *= .0;
            error_rate[i] += fabs(error[i])>1;
            if (i==0) error[i] *= 5;
            rms += error[i]*error[i];
            /*error[i] = error[i]/(1+fabs(error[i]));*/
        }
        /* Back-propagate error */
        for (i=0;i<outDim;i++)
        {
            double grad = 1-netOut[i]*netOut[i];
            W1_grad[i*(hiddenDim+1)] += error[i]*grad;
            for (j=0;j<hiddenDim;j++)
                W1_grad[i*(hiddenDim+1)+j+1] += grad*error[i]*hidden[j];
        }
        for (i=0;i<hiddenDim;i++)
        {
            double grad;
            grad = 0;
            for (j=0;j<outDim;j++)
                grad += error[j]*W1[j*(hiddenDim+1)+i+1];
            grad *= 1-hidden[i]*hidden[i];
            W0_grad[i*(inDim+1)] += grad;
            for (j=0;j<inDim;j++)
                W0_grad[i*(inDim+1)+j+1] += grad*inp[j];
        }
    }
    return rms;
}

#define NB_THREADS 8

sem_t sem_begin[NB_THREADS];
sem_t sem_end[NB_THREADS];

struct GradientArg {
    int id;
    int done;
    MLPTrain *net;
    float *inputs;
    float *outputs;
    int nbSamples;
    double *W0_grad;
    double *W1_grad;
    double rms;
    double error_rate[MAX_OUT];
};

void *gradient_thread_process(void *_arg)
{
    int W0_size, W1_size;
    struct GradientArg *arg = _arg;
    int *topo = arg->net->topo;
    W0_size = (topo[0]+1)*topo[1];
    W1_size = (topo[1]+1)*topo[2];
    double W0_grad[W0_size];
    double W1_grad[W1_size];
    arg->W0_grad = W0_grad;
    arg->W1_grad = W1_grad;
    while (1)
    {
        sem_wait(&sem_begin[arg->id]);
        if (arg->done)
            break;
        arg->rms = compute_gradient(arg->net, arg->inputs, arg->outputs, arg->nbSamples, arg->W0_grad, arg->W1_grad, arg->error_rate);
        sem_post(&sem_end[arg->id]);
    }
    fprintf(stderr, "done\n");
    return NULL;
}

float mlp_train_backprop(MLPTrain *net, float *inputs, float *outputs, int nbSamples, int nbEpoch, float rate)
{
    int i, j;
    int e;
    float best_rms = 1e10;
    int inDim, outDim, hiddenDim;
    int *topo;
    double *W0, *W1, *best_W0, *best_W1;
    double *W0_grad, *W1_grad;
    double *W0_oldgrad, *W1_oldgrad;
    double *W0_rate, *W1_rate;
    double *best_W0_rate, *best_W1_rate;
    int W0_size, W1_size;
    topo = net->topo;
    W0_size = (topo[0]+1)*topo[1];
    W1_size = (topo[1]+1)*topo[2];
    struct GradientArg args[NB_THREADS];
    pthread_t thread[NB_THREADS];
    int samplePerPart = nbSamples/NB_THREADS;
    int count_worse=0;
    int count_retries=0;

    topo = net->topo;
    inDim = net->topo[0];
    hiddenDim = net->topo[1];
    outDim = net->topo[2];
    W0 = net->weights[0];
    W1 = net->weights[1];
    best_W0 = net->best_weights[0];
    best_W1 = net->best_weights[1];
    W0_grad = malloc(W0_size*sizeof(double));
    W1_grad = malloc(W1_size*sizeof(double));
    W0_oldgrad = malloc(W0_size*sizeof(double));
    W1_oldgrad = malloc(W1_size*sizeof(double));
    W0_rate = malloc(W0_size*sizeof(double));
    W1_rate = malloc(W1_size*sizeof(double));
    best_W0_rate = malloc(W0_size*sizeof(double));
    best_W1_rate = malloc(W1_size*sizeof(double));
    memset(W0_grad, 0, W0_size*sizeof(double));
    memset(W0_oldgrad, 0, W0_size*sizeof(double));
    memset(W1_grad, 0, W1_size*sizeof(double));
    memset(W1_oldgrad, 0, W1_size*sizeof(double));

    rate /= nbSamples;
    for (i=0;i<hiddenDim;i++)
        for (j=0;j<inDim+1;j++)
            W0_rate[i*(inDim+1)+j] = rate*net->in_rate[j];
    for (i=0;i<W1_size;i++)
        W1_rate[i] = rate;

    for (i=0;i<NB_THREADS;i++)
    {
        args[i].net = net;
        args[i].inputs = inputs+i*samplePerPart*inDim;
        args[i].outputs = outputs+i*samplePerPart*outDim;
        args[i].nbSamples = samplePerPart;
        args[i].id = i;
        args[i].done = 0;
        sem_init(&sem_begin[i], 0, 0);
        sem_init(&sem_end[i], 0, 0);
        pthread_create(&thread[i], NULL, gradient_thread_process, &args[i]);
    }
    for (e=0;e<nbEpoch;e++)
    {
        double rms=0;
        double error_rate[2] = {0,0};
        for (i=0;i<NB_THREADS;i++)
        {
            sem_post(&sem_begin[i]);
        }
        memset(W0_grad, 0, W0_size*sizeof(double));
        memset(W1_grad, 0, W1_size*sizeof(double));
        for (i=0;i<NB_THREADS;i++)
        {
            sem_wait(&sem_end[i]);
            rms += args[i].rms;
            error_rate[0] += args[i].error_rate[0];
            error_rate[1] += args[i].error_rate[1];
            for (j=0;j<W0_size;j++)
                W0_grad[j] += args[i].W0_grad[j];
            for (j=0;j<W1_size;j++)
                W1_grad[j] += args[i].W1_grad[j];
        }

        float mean_rate = 0, min_rate = 1e10;
        rms = (rms/(outDim*nbSamples));
        error_rate[0] = (error_rate[0]/(nbSamples));
        error_rate[1] = (error_rate[1]/(nbSamples));
        fprintf (stderr, "%f %f (%f %f) ", error_rate[0], error_rate[1], rms, best_rms);
        if (rms < best_rms)
        {
            best_rms = rms;
            for (i=0;i<W0_size;i++)
            {
                best_W0[i] = W0[i];
                best_W0_rate[i] = W0_rate[i];
            }
            for (i=0;i<W1_size;i++)
            {
                best_W1[i] = W1[i];
                best_W1_rate[i] = W1_rate[i];
            }
            count_worse=0;
            count_retries=0;
        } else {
            count_worse++;
            if (count_worse>30)
            {
                count_retries++;
                count_worse=0;
                for (i=0;i<W0_size;i++)
                {
                    W0[i] = best_W0[i];
                    best_W0_rate[i] *= .7;
                    if (best_W0_rate[i]<1e-15) best_W0_rate[i]=1e-15;
                    W0_rate[i] = best_W0_rate[i];
                    W0_grad[i] = 0;
                }
                for (i=0;i<W1_size;i++)
                {
                    W1[i] = best_W1[i];
                    best_W1_rate[i] *= .8;
                    if (best_W1_rate[i]<1e-15) best_W1_rate[i]=1e-15;
                    W1_rate[i] = best_W1_rate[i];
                    W1_grad[i] = 0;
                }
            }
        }
        if (count_retries>10)
            break;
        for (i=0;i<W0_size;i++)
        {
            if (W0_oldgrad[i]*W0_grad[i] > 0)
                W0_rate[i] *= 1.01;
            else if (W0_oldgrad[i]*W0_grad[i] < 0)
                W0_rate[i] *= .9;
            mean_rate += W0_rate[i];
            if (W0_rate[i] < min_rate)
                min_rate = W0_rate[i];
            if (W0_rate[i] < 1e-15)
                W0_rate[i] = 1e-15;
            /*if (W0_rate[i] > .01)
                W0_rate[i] = .01;*/
            W0_oldgrad[i] = W0_grad[i];
            W0[i] += W0_grad[i]*W0_rate[i];
        }
        for (i=0;i<W1_size;i++)
        {
            if (W1_oldgrad[i]*W1_grad[i] > 0)
                W1_rate[i] *= 1.01;
            else if (W1_oldgrad[i]*W1_grad[i] < 0)
                W1_rate[i] *= .9;
            mean_rate += W1_rate[i];
            if (W1_rate[i] < min_rate)
                min_rate = W1_rate[i];
            if (W1_rate[i] < 1e-15)
                W1_rate[i] = 1e-15;
            W1_oldgrad[i] = W1_grad[i];
            W1[i] += W1_grad[i]*W1_rate[i];
        }
        mean_rate /= (topo[0]+1)*topo[1] + (topo[1]+1)*topo[2];
        fprintf (stderr, "%g %d", mean_rate, e);
        if (count_retries)
            fprintf(stderr, " %d", count_retries);
        fprintf(stderr, "\n");
        if (stopped)
            break;
    }
    for (i=0;i<NB_THREADS;i++)
    {
        args[i].done = 1;
        sem_post(&sem_begin[i]);
        pthread_join(thread[i], NULL);
        fprintf (stderr, "joined %d\n", i);
    }
    free(W0_grad);
    free(W0_oldgrad);
    free(W1_grad);
    free(W1_oldgrad);
    free(W0_rate);
    free(best_W0_rate);
    free(W1_rate);
    free(best_W1_rate);
    return best_rms;
}

int main(int argc, char **argv)
{
    int i, j;
    int nbInputs;
    int nbOutputs;
    int nbHidden;
    int nbSamples;
    int nbEpoch;
    int nbRealInputs;
    unsigned int seed;
    int ret;
    float rms;
    float *inputs;
    float *outputs;
    if (argc!=6)
    {
        fprintf (stderr, "usage: mlp_train <inputs> <hidden> <outputs> <nb samples> <nb epoch>\n");
        return 1;
    }
    nbInputs = atoi(argv[1]);
    nbHidden = atoi(argv[2]);
    nbOutputs = atoi(argv[3]);
    nbSamples = atoi(argv[4]);
    nbEpoch = atoi(argv[5]);
    nbRealInputs = nbInputs;
    inputs = malloc(nbInputs*nbSamples*sizeof(*inputs));
    outputs = malloc(nbOutputs*nbSamples*sizeof(*outputs));

    seed = time(NULL);
    /*seed = 1452209040;*/
    fprintf (stderr, "Seed is %u\n", seed);
    srand(seed);
    build_tansig_table();
    signal(SIGTERM, handler);
    signal(SIGINT, handler);
    signal(SIGHUP, handler);
    for (i=0;i<nbSamples;i++)
    {
        for (j=0;j<nbRealInputs;j++)
            ret = scanf(" %f", &inputs[i*nbInputs+j]);
        for (j=0;j<nbOutputs;j++)
            ret = scanf(" %f", &outputs[i*nbOutputs+j]);
        if (feof(stdin))
        {
            nbSamples = i;
            break;
        }
    }
    int topo[3] = {nbInputs, nbHidden, nbOutputs};
    MLPTrain *net;

    fprintf (stderr, "Got %d samples\n", nbSamples);
    net = mlp_init(topo, 3, inputs, outputs, nbSamples);
    rms = mlp_train_backprop(net, inputs, outputs, nbSamples, nbEpoch, 1);
    printf ("#ifdef HAVE_CONFIG_H\n");
    printf ("#include \"config.h\"\n");
    printf ("#endif\n\n");
    printf ("#include \"mlp.h\"\n\n");
    printf ("/* RMS error was %f, seed was %u */\n\n", rms, seed);
    printf ("static const float weights[%d] = {\n", (topo[0]+1)*topo[1] + (topo[1]+1)*topo[2]);
    printf ("\n/* hidden layer */\n");
    for (i=0;i<(topo[0]+1)*topo[1];i++)
    {
        printf ("%gf,", net->weights[0][i]);
        if (i%5==4)
            printf("\n");
        else
            printf(" ");
    }
    printf ("\n/* output layer */\n");
    for (i=0;i<(topo[1]+1)*topo[2];i++)
    {
        printf ("%gf,", net->weights[1][i]);
        if (i%5==4)
            printf("\n");
        else
            printf(" ");
    }
    printf ("};\n\n");
    printf ("static const int topo[3] = {%d, %d, %d};\n\n", topo[0], topo[1], topo[2]);
    printf ("const MLP net = {\n");
    printf ("    3,\n");
    printf ("    topo,\n");
    printf ("    weights\n};\n");
    return 0;
}
