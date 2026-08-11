using RayTracer.Materials;
using System;
using System.Runtime.Intrinsics;

namespace RayTracer.Objects
{
    /// <summary>
    /// A plane is, conceptually, a sheet that extends infinitely in all directions.
    /// </summary>
    public class InfinitePlane : DrawableSceneObject
    {
        private Vector128<float> normalDirection;
        protected Vector128<float> uDirection;
        protected Vector128<float> vDirection;
        private float cellWidth;

        /// <summary>
        /// Constructs a plane with the properties provided
        /// </summary>
        /// <param name="position">The position of the plane's center</param>
        /// <param name="material">The plane's material</param>
        /// <param name="normalDirection">The normal direction of the plane</param>
        /// <param name="cellWidth">The width of a cell in the plane, used for texture coordinate mapping.</param>
        public InfinitePlane(Vector128<float> position, Material material, Vector128<float> normalDirection, float cellWidth)
            : base(position, material)
        {
            this.normalDirection = normalDirection.Normalize();
            if (normalDirection == Util.ForwardVector)
            {
                this.uDirection = -Util.RightVector;
            }
            else if (normalDirection == -Util.ForwardVector)
            {
                this.uDirection = Util.RightVector;
            }
            else
            {
                this.uDirection = Util.CrossProduct(normalDirection, Util.ForwardVector).Normalize();
            }

            this.vDirection = -Util.CrossProduct(normalDirection, uDirection).Normalize();
            this.cellWidth = cellWidth;
        }

        public override bool TryCalculateIntersection(Ray ray, out Intersection intersection)
        {
            intersection = new Intersection();

            Vector128<float> vecDirection = ray.Direction;
            Vector128<float> rayToPlaneDirection = ray.Origin - this.Position;

            //float D = Vector128.Dot(this.normalDirection, vecDirection);
            //float N = -Vector128.Dot(this.normalDirection, rayToPlaneDirection);
            float D = this.normalDirection.DotR(vecDirection);
            float N = -this.normalDirection.DotR(rayToPlaneDirection);

            if (Math.Abs(D) <= .0005f)
            {
                return false;
            }

            float sI = N / D;
            if (sI < 0 || sI > ray.Distance) // Behind or out of range
            {
                return false;
            }

            var intersectionPoint = ray.Origin + (Vector128.Create(sI) * vecDirection);
            var uv = this.GetUVCoordinate(intersectionPoint);

            var color = Material.GetDiffuseColorAtCoordinates(uv);

            intersection = new Intersection(intersectionPoint, this.normalDirection, ray.Direction, this, color, (ray.Origin - intersectionPoint).Magnitude());
            return true;
        }

        public override UVCoordinate GetUVCoordinate(Vector128<float> position)
        {
            var uvPosition = this.Position + position;

            //var uMag = Vector128.Dot(uvPosition, uDirection);
            var uMag = uvPosition.DotR(uDirection);
            var u = (Vector128.Create(uMag) * uDirection).Magnitude();
            if (uMag < 0)
            {
                u += cellWidth / 2f;
            }
            u = (u % cellWidth) / cellWidth;

            //var vMag = Vector128.Dot(uvPosition, vDirection);
            var vMag = uvPosition.DotR(vDirection);
            var v = (Vector128.Create(vMag) * vDirection).Magnitude();
            if (vMag < 0)
            {
                v += cellWidth / 2f;
            }
            v = (v % cellWidth) / cellWidth;

            return new UVCoordinate(u, v);
        }
    }
}
