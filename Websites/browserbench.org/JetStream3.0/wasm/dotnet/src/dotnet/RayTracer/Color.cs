using System.Runtime.Intrinsics;

namespace RayTracer
{
    /// <summary>
    /// Represents a color, with components Red, Green, Blue, and Alpha between 0 (min) and 1 (max).
    /// </summary>
    public struct Color
    {
        Vector128<float> vector;

        /// <summary> The color's red component, between 0.0 and 1.0 </summary>
        public float R { get { return vector.GetElement(0); } }

        /// <summary> The color's green component, between 0.0 and 1.0 </summary>
        public float G { get { return vector.GetElement(1); } }

        /// <summary> The color's blue component, between 0.0 and 1.0 </summary>
        public float B { get { return vector.GetElement(2); } }

        /// <summary> The color's alpha component, between 0.0 and 1.0 </summary>
        public float A { get { return vector.GetElement(3); } }

        /// <summary>
        /// Constructs a color from the given component values.
        /// </summary>
        /// <param name="r">The color's red value</param>
        /// <param name="g">The color's green value</param>
        /// <param name="b">The color's blue value</param>
        /// <param name="a">The color's alpha value</param>
        public Color(float r, float g, float b, float a)
        {
            this.vector = Vector128.Create(r, g, b, a);
        }

        private Color(Vector128<float> vec)
        {
            this.vector = vec;
        }

        public static readonly Color Red = new Color(1, 0, 0, 1);
        public static readonly Color Green = new Color(0, 1, 0, 1);
        public static readonly Color Blue = new Color(0, 0, 1, 1);
        public static readonly Color Purple = new Color(1, 0, 1, 1);
        public static readonly Color White = new Color(1, 1, 1, 1);
        public static readonly Color Black = new Color(0, 0, 0, 1);
        public static readonly Color Yellow = new Color(1, 1, 0, 1);
        public static readonly Color Grey = new Color(.6f, .6f, .6f, 1);
        public static readonly Color Clear = new Color(1, 1, 1, 0);
        public static readonly Color DarkGrey = new Color(.8f, .8f, .85f, 1);
        public static readonly Color Sky = new Color(102f / 255f, 152f / 255f, 1f, 1f);
        public static readonly Color Zero = new Color(0f, 0f, 0f, 0f);
        public static readonly Color Silver = System.Drawing.Color.Silver;
        public static readonly Color Orange = System.Drawing.Color.Orange;
        public static readonly Color DarkGreen = System.Drawing.Color.DarkGreen;

        public override string ToString()
        {
            return string.Format("Color: [{0}, {1}, {2}, {3}]", this.R, this.G, this.B, this.A);
        }

        /// <summary>
        /// Returns a new color whose components are the average of the components of first and second
        /// </summary>
        public static Color Average(Color first, Color second)
        {
            return new Color((first.vector + second.vector) * .5f);
        }

        /// <summary>
        /// Linearly interpolates from one color to another based on t.
        /// </summary>
        /// <param name="from">The first color value</param>
        /// <param name="to">The second color value</param>
        /// <param name="t">The weight value. At t = 0, "from" is returned, at t = 1, "to" is returned.</param>
        /// <returns></returns>
        public static Color Lerp(Color from, Color to, float t)
        {
            t = Util.Clamp(t, 0f, 1f);

            return from * (1 - t) + to * t;
        }

        public static Color operator *(Color color, float factor)
        {
            return new Color(color.vector * factor);
        }
        public static Color operator *(float factor, Color color)
        {
            return new Color(color.vector * factor);
        }
        public static Color operator *(Color left, Color right)
        {
            return new Color(left.vector * right.vector);
        }

        public static implicit operator System.Drawing.Color(Color c)
        {
            var colorLimited = c.Limited;
            try
            {
                return System.Drawing.Color.FromArgb((int)(255 * colorLimited.A), (int)(255 * colorLimited.R), (int)(255 * colorLimited.G), (int)(255 * colorLimited.B));
            }
            catch
            {
                return new System.Drawing.Color();
            }
        }

        public static implicit operator Color(System.Drawing.Color c)
        {
            try
            {
                return new Color(c.R / 255f, c.G / 255f, c.B / 255f, c.A / 255f);
            }
            catch
            {
                return new Color();
            }
        }

        /// <summary>
        /// Returns this color with the component values clamped from 0 to 1.
        /// </summary>
        public Color Limited
        {
            get
            {
                var r = Util.Clamp(R, 0, 1);
                var g = Util.Clamp(G, 0, 1);
                var b = Util.Clamp(B, 0, 1);
                var a = Util.Clamp(A, 0, 1);
                return new Color(r, g, b, a);
            }
        }

        public static Color operator +(Color left, Color right)
        {
            return new Color(left.R + right.R, left.G + right.G, left.B + right.B, left.A + right.A);
        }

        public static Color operator -(Color left, Color right)
        {
            return new Color(left.R - right.R, left.G - right.G, left.B - right.B, left.A - right.A);
        }

        /// <summary>
        /// Returns a BGRA32 integer representation of the color
        /// </summary>
        /// <param name="color">The color object to convert</param>
        /// <returns>An integer value whose 4 bytes each represent a single BGRA component value from 0-255</returns>
        public static int ToBGRA32(Color color)
        {
            byte r = (byte)(255 * color.R);
            byte g = (byte)(255 * color.G);
            byte b = (byte)(255 * color.B);
            byte a = (byte)(255 * color.A);

            return (r << 16) | (g << 8) | (b << 0) | (a << 24);
        }
    }
}
