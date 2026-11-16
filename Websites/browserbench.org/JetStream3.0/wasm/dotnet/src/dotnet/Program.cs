using System;
using System.Runtime.InteropServices.JavaScript;
using System.Threading.Tasks;

[assembly:System.Runtime.Versioning.SupportedOSPlatform("browser")]

static class Program
{
    static void Main()
    {
        // Noop
    }
}

static partial class Interop
{
    static BenchTask[] tasks =
    [
        new Sample.ExceptionsTask(),
        new Sample.JsonTask(),
        new Sample.StringTask()
    ];

    [JSExport]
    public static async Task RunIteration(int benchTasksBatchSize, int sceneWidth, int sceneHeight, int hardwareConcurrency)
    {
        // BenchTasks
        for (int i = 0; i < tasks.Length; i++)
        {
            var task = tasks[i];
            for (int j = 0; j < task.Measurements.Length; j++)
            {
                await task.RunBatch(i, benchTasksBatchSize);
            }
        }

        // RayTracer
        MainJS.PrepareToRender(sceneWidth, sceneHeight, hardwareConcurrency);
        await MainJS.Render();
    }
}