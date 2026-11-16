namespace RayTracer.Materials
{
    /// <summary>
    /// A material resembling a checkerboard, with two distinct colors alternating in a checkerboard-like grid pattern.
    /// </summary>
    public class CheckerboardMaterial : Material
    {
        Color evenColor;
        Color oddColor;

        /// <summary>
        /// Constructs a CheckerboardMaterial object with the given properties
        /// </summary>
        /// <param name="even">The color to use in even-number cells</param>
        /// <param name="odd">The color to use in odd-numer cells</param>
        /// <param name="opacity">The percentage of light that is absorbed by the material</param>
        /// <param name="reflectivity">The percentage of light that is reflected by the material</param>
        /// <param name="refractivity">The amount of refraction occurring on rays passing through the material</param>
        /// <param name="glossiness">The glossiness of the material, which impacts shiny specular highlighting</param>
        public CheckerboardMaterial(Color even, Color odd, float opacity, float reflectivity, float refractivity, float glossiness)
            : base(reflectivity, refractivity, opacity, glossiness)
        {
            this.evenColor = even;
            this.oddColor = odd;
        }

        /// <summary>
        /// Constructs a generic CheckerboardMaterial, with alternating white and black tiles.
        /// </summary>
        public CheckerboardMaterial() : this(Color.White, Color.Black, 1.0f, .35f, 0.0f, 3.0f) { }

        public override Color GetDiffuseColorAtCoordinates(float u, float v)
        {
            if ((u <= 0.5f && v <= .5f) || (u > 0.5f && v > 0.5f))
            {
                return evenColor;
            }
            else
            {
                return oddColor;
            }
        }

        public override Color GetSpecularColorAtCoordinates(float u, float v)
        {
            Color color;
            if ((u <= 0.5f && v <= .5f) || (u > 0.5f && v > 0.5f))
            {
                color = evenColor;
            }
            else
            {
                color = oddColor;
            }

            return Color.Lerp(color, Color.White, Glossiness / 10.0f);
        }
    }
}
