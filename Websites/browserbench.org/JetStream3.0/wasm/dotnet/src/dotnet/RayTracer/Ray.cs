using System;
using System.Runtime.Intrinsics;

namespace RayTracer
{
    /// <summary>
    /// Represents a ray primitive. Used as a basis for intersection calculations.
    /// </summary>
    public struct Ray
    {
        public readonly Vector128<float> Origin;
        public readonly Vector128<float> Direction;
        public readonly float Distance;
        public Ray(Vector128<float> start, Vector128<float> direction, float distance)
        {
            this.Origin = start;
            this.Direction = direction.Normalize();
            this.Distance = distance;
        }
        public Ray(Vector128<float> start, Vector128<float> direction) : this(start, direction, float.PositiveInfinity) { }
    }
}
