namespace RayTracer.Materials
{
    /// <summary>
    /// A solid material represents a mono-colored object.
    /// </summary>
    public class SolidMaterial : Material
    {
        public Color DiffuseColor { get; set; }
        public Color SpecularColor { get; set; }

        /// <summary>
        /// Constructs a solid material with the given color.
        /// </summary>
        public SolidMaterial(Color color) : this(color, 0.0f, 0.0f, 1.0f, 1.0f) { }
        /// <summary>
        /// Constructs a CheckerboardMaterial object with the given properties
        /// </summary>
        /// <param name="even">The color to use in even-number cells</param>
        /// <param name="odd">The color to use in odd-numer cells</param>
        /// <param name="opacity">The percentage of light that is absorbed by the material</param>
        /// <param name="reflectivity">The percentage of light that is reflected by the material</param>
        /// <param name="refractivity">The amount of refraction occurring on rays passing through the material</param>
        /// <param name="glossiness">The glossiness of the material, which impacts shiny specular highlighting</param>
        public SolidMaterial(Color color, float reflectivity = 0.0f, float refractivity = 0.0f, float opacity = 1.0f, float glossiness = 1.0f)
            : base(reflectivity, refractivity, opacity, glossiness)
        {
            this.DiffuseColor = color;
            this.SpecularColor = Color.Lerp(this.DiffuseColor, Color.White, this.Glossiness / 10.0f);
        }

        public override Color GetDiffuseColorAtCoordinates(float u, float v)
        {
            return this.DiffuseColor;
        }
        public override Color GetSpecularColorAtCoordinates(float u, float v)
        {
            return this.SpecularColor;
        }
    }
}
