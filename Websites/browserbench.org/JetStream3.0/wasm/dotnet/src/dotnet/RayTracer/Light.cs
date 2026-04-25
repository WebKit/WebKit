using System.Runtime.Intrinsics;

namespace RayTracer.Objects
{
    /// <summary>
    /// Represents a point light in a scene with a constant color and intensity
    /// </summary>
    public class Light : SceneObjectBase
    {
        public Color Color { get; set; }
        private float intensity;

        /// <summary>
        /// Gets the intensity of a light at the given distance
        /// </summary>
        /// <param name="distance"></param>
        /// <returns></returns>
        public float GetIntensityAtDistance(float distance)
        {
            if (distance > intensity) return 0;
            else
            {
                var percentOfMax = (intensity - distance) / distance;

                var percent = 1 - (distance / intensity);
                return percent;
            }
        }

        /// <summary>
        /// Constructs a light at the given position, with the given light color and intensity.
        /// </summary>
        /// <param name="position">World position of the light</param>
        /// <param name="intensity">Light intensity</param>
        /// <param name="color">Light color</param>
        public Light(Vector128<float> position, float intensity, Color color)
            : base(position)
        {
            this.Color = color;
            this.intensity = intensity;
        }
    }
}
