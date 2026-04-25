using System.Runtime.InteropServices.JavaScript;
using System;
using RayTracer;
using System.Threading.Tasks;

public partial class MainJS
{
    struct SceneEnvironment {
        public int Width;
        public int Height;
        public int HardwareConcurrency;
        public byte[] rgbaRenderBuffer;
        public Scene Scene;
    }

    static SceneEnvironment sceneEnvironment;

    internal static Scene ConfigureScene()
    {
        var scene = Scene.TwoPlanes;
        scene.Camera.ReflectionDepth = 5;
        scene.Camera.FieldOfView = 120;
        return scene;
    }

    internal static void PrepareToRender(int sceneWidth, int sceneHeight, int hardwareConcurrency)
    {
        sceneEnvironment.Width = sceneWidth;
        sceneEnvironment.Height = sceneHeight;
        sceneEnvironment.HardwareConcurrency = hardwareConcurrency;
        sceneEnvironment.Scene = ConfigureScene();
        sceneEnvironment.rgbaRenderBuffer = new byte[sceneWidth * sceneHeight * 4];
    }

    internal static Task Render()
    {
        return sceneEnvironment.Scene.Camera.RenderScene(sceneEnvironment.Scene, sceneEnvironment.rgbaRenderBuffer, sceneEnvironment.Width, sceneEnvironment.Height, sceneEnvironment.HardwareConcurrency);
    }
}
