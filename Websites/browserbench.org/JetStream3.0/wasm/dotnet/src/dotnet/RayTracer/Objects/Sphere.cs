using RayTracer.Materials;
using System;
using System.Runtime.Intrinsics;

namespace RayTracer.Objects
{
    /// <summary>
    /// A three-dimensional object whose surface boundaries are a fixed distance from a central origin in every direction.
    /// </summary>
    public class Sphere : DrawableSceneObject
    {
        /// <summary>
        /// The distance from the sphere's origin to its surface.
        /// </summary>
        public float Radius { get; set; }
        /// <summary>
        /// Constructs a sphere at the given position, with the given material and radius
        /// </summary>
        /// <param name="position">The sphere's origin position</param>
        /// <param name="material">The sphere's surface material</param>
        /// <param name="radius">The radius of the sphere</param>
        public Sphere(Vector128<float> position, Material material, float radius)
            : base(position, material)
        {
            this.Radius = radius;
        }
        public override bool TryCalculateIntersection(Ray ray, out Intersection intersection)
        {
            intersection = new Intersection();

            Vector128<float> rayToSphere = ray.Origin - this.Position;
            //float B = Vector128.Dot(rayToSphere, ray.Direction);
            //float C = Vector128.Dot(rayToSphere, rayToSphere) - (Radius * Radius);
            float B = rayToSphere.DotR(ray.Direction);
            float C = rayToSphere.DotR(rayToSphere) - (Radius * Radius);
            float D = B * B - C;

            if (D > 0)
            {
                var distance = -B - (float)Math.Sqrt(D);
                var hitPosition = ray.Origin + (ray.Direction * Vector128.Create(distance));
                var normal = hitPosition - this.Position;
                UVCoordinate uv = this.GetUVCoordinate(hitPosition);
                intersection = new Intersection(hitPosition, normal, ray.Direction, this, Material.GetDiffuseColorAtCoordinates(uv), distance);
                return true;
            }
            else
            {
                return false;
            }
        }

        public override UVCoordinate GetUVCoordinate(Vector128<float> hitPosition)
        {
            var toCenter = (hitPosition - this.Position).Normalize();

            float u = (float)(0.5 + ((Math.Atan2(toCenter.Z(), toCenter.X())) / (2 * Math.PI)));
            float v = (float)(0.5 - (Math.Asin(toCenter.Y())) / Math.PI);
            
            return new UVCoordinate(u, v);
        }
    }
}
