using RayTracer.Materials;
using System.Runtime.Intrinsics;

namespace RayTracer.Objects
{
    /// <summary>
    /// The base class for renderable scene objects
    /// </summary>
    public abstract class DrawableSceneObject : SceneObjectBase
    {
        /// <summary>
        /// Determines whether the given ray intersects with this scene object.
        /// </summary>
        /// <param name="ray">The ray to test</param>
        /// <param name="intersection">If the ray intersects, this contains the intersection object</param>
        /// <returns>A value indicating whether the ray intersects with this object</returns>
        public abstract bool TryCalculateIntersection(Ray ray, out Intersection intersection);
        /// <summary>
        /// Gets the UV texture coordinate of a particular position on the surface of this object
        /// </summary>
        /// <param name="position"></param>
        /// <returns></returns>
        public abstract UVCoordinate GetUVCoordinate(Vector128<float> position);
        
        public Material Material { get; set; }

        public DrawableSceneObject(Vector128<float> position, Material material)
            : base(position)
        {
            this.Material = material;
        }
    }
}
