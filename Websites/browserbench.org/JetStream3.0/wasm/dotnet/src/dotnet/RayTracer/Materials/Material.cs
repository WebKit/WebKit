namespace RayTracer.Materials
{
    /// <summary>
    /// A material represents a surface's physical material, and represents how it appears at any given point on the surface.
    /// </summary>
    public abstract class Material
    {
        /// <summary>
        /// Returns the diffuse color of a material at the given UV coordinates.
        /// </summary>
        public abstract Color GetDiffuseColorAtCoordinates(float u, float v);
        /// <summary>
        /// Returns the specular color of a material at the given UV coordinates.
        /// </summary>
        public Color GetDiffuseColorAtCoordinates(UVCoordinate uv)
        {
            return GetDiffuseColorAtCoordinates(uv.U, uv.V);
        }
        /// <summary>
        /// Returns the specular color of a material at the given UV coordinates.
        /// </summary>
        public abstract Color GetSpecularColorAtCoordinates(float u, float v);
        /// <summary>
        /// Returns the specular color of a material at the given UV coordinates.
        /// </summary>
        public Color GetSpecularColorAtCoordinates(UVCoordinate uv)
        {
            return GetSpecularColorAtCoordinates(uv.U, uv.V);
        }
        /// <summary>
        /// The reflectivity of a material is the percentage of light reflected.
        /// </summary>
        public float Reflectivity { get; private set; }
        /// <summary>
        /// The refractivity of a material impacts how much light is bent when passing through it.
        /// </summary>
        public float Refractivity { get; private set; }
        /// <summary>
        /// The opacity of a material is the percentage of light absorbed by it.
        /// </summary>
        public float Opacity { get; private set; }
        /// <summary>
        /// Returns the amount of light that is passed through the object. 1 - Opacity.
        /// </summary>
        public float Transparency { get { return 1f - Opacity; } }
        /// <summary>
        /// The glossiness of a material impacts how shiny the specular highlights on it are.
        /// </summary>
        public float Glossiness { get; private set; }

        protected Material(float reflectivity, float refractivity, float opacity, float glossiness)
        {
            this.Reflectivity = reflectivity;
            this.Refractivity = refractivity;
            this.Opacity = opacity;
            this.Glossiness = glossiness;
        }
    }
}
