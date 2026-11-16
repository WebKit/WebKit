using System.Runtime.Intrinsics;

namespace RayTracer
{
    /// <summary>
    /// The base class for all scene objects, which must contain a world-space position in the scene.
    /// </summary>
    public abstract class SceneObjectBase
    {
        /// <summary>
        /// The world-space position of the scene object
        /// </summary>
        public Vector128<float> Position { get; set; }
        public SceneObjectBase(Vector128<float> position)
        {
            this.Position = position;
        }
    }
}
