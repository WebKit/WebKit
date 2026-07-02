// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Runtime.InteropServices.JavaScript;

public abstract class BenchTask
{
    public abstract string Name { get; }
    readonly List<Result> results = new();
    public Regex pattern;

    public virtual bool BrowserOnly => false;

    public async Task RunInitialSamples(int measurementIdx)
    {
        var measurement = Measurements[measurementIdx];
        await measurement.RunInitialSamples();
    }
    
    public async Task RunBatch(int measurementIdx, int batchSize)
    {
        var measurement = Measurements[measurementIdx];
        await measurement.BeforeBatch();
        await measurement.RunBatch(batchSize);
        await measurement.AfterBatch();
    }

    public virtual void Initialize() { }

    public abstract Measurement[] Measurements { get; }

    public class Result
    {
        public TimeSpan span;
        public int steps;
        public string taskName;
        public string measurementName;

        public override string ToString() => $"{taskName}, {measurementName} count: {steps}, per call: {span.TotalMilliseconds / steps}ms, total: {span.TotalSeconds}s";
    }

    public abstract class Measurement
    {
        protected int currentStep = 0;
        public abstract string Name { get; }

        public virtual int InitialSamples => 2;
        public virtual int NumberOfRuns => 5;
        public virtual int RunLength => 5000;
        public virtual Task<bool> IsEnabled() => Task.FromResult(true);

        public virtual Task BeforeBatch() { return Task.CompletedTask; }

        public virtual Task AfterBatch() { return Task.CompletedTask; }

        public virtual void RunStep() { }
        public virtual async Task RunStepAsync() { await Task.CompletedTask; }

        public virtual bool HasRunStepAsync => false;

        protected virtual int CalculateSteps(int milliseconds, TimeSpan initTs, int initialSamples)
        {
            return (int)(milliseconds * initialSamples / Math.Max(1.0, initTs.TotalMilliseconds));
        }

        public async Task RunInitialSamples()
        {
            int initialSamples = InitialSamples;
            try
            {
                // run one to eliminate possible startup overhead and do GC collection
                if (HasRunStepAsync)
                    await RunStepAsync();
                else
                    RunStep();

                GC.Collect();

                if (HasRunStepAsync)
                    await RunStepAsync();
                else
                    RunStep();

                if (initialSamples > 1)
                {
                    GC.Collect();

                    for (currentStep = 0; currentStep < initialSamples; currentStep++)
                    {
                        if (HasRunStepAsync)
                            await RunStepAsync();
                        else
                            RunStep();
                    }
                }
            }
            catch (Exception)
            {
            }
        }

        public async Task RunBatch(int batchSize)
        {
            int initialSamples = InitialSamples;
            try
            {
                for (currentStep = 0; currentStep < initialSamples * batchSize; currentStep++)
                {
                    if (HasRunStepAsync)
                        await RunStepAsync();
                    else
                        RunStep();
                }
            }
            catch (Exception)
            {
            }
        }
    }
}