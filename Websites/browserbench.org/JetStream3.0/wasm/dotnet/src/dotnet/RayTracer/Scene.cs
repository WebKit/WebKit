using RayTracer.Materials;
using RayTracer.Objects;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.Intrinsics;

namespace RayTracer
{
    /// <summary>
    /// A container object holding drawable objects, lights, and a camera.
    /// </summary>
    public class Scene
    {
        /// <summary>
        /// The set of drawable objects in the scene
        /// </summary>
        public List<DrawableSceneObject> DrawableObjects { get; set; }
        /// <summary>
        /// The set of lights in the scene
        /// </summary>
        public List<Light> Lights { get; set; }
        /// <summary>
        /// The camera used to render the scene
        /// </summary>
        public Camera Camera { get; set; }
        /// <summary>
        /// The background color, used in rendering when a ray intersects no objects
        /// </summary>
        public Color BackgroundColor { get; set; }

        /// <summary>
        /// The intensity of the ambient light source
        /// </summary>
        public readonly float AmbientLightIntensity;
        /// <summary>
        /// The ambient light color in the scene.
        /// </summary>
        public readonly Color AmbientLightColor;

        public Scene() : this(Color.Blue, Color.White, .25f) { }
        public Scene(Color backgroundColor) : this(backgroundColor, Color.White, .25f) { }
        public Scene(Color backgroundColor, Color ambientLightColor, float ambientLightIntensity)
        {
            this.DrawableObjects = new List<DrawableSceneObject>();
            this.Lights = new List<Light>();

            this.BackgroundColor = backgroundColor;
            this.AmbientLightIntensity = ambientLightIntensity;
            this.AmbientLightColor = ambientLightColor;
        }

        public static Scene DefaultScene
        {
            get
            {
                var scene = new Scene(Color.Sky, Color.White, 0.05f);
                scene.Lights.Add(new Light(Vector128.Create(3f, 10f, 3f, 0), 1800.0f, Color.White));
                scene.Lights.Add(new Light(Vector128.Create(-7, -2, 4.5f, 0), 7.0f, Color.Blue));
                scene.DrawableObjects.Add(new Sphere(Vector128.Create(-7, -2, 2.5f, 0), new SolidMaterial(Color.Blue, opacity: .75f, reflectivity: 0.0f), 0.333f));

                scene.DrawableObjects.Add(new InfinitePlane(Util.UpVector * Vector128.Create(-5f),
                    new CheckerboardMaterial(Color.DarkGreen, Color.Silver, 1.0f, .05f, 0.0f, 5.0f),
                        Util.UpVector,
                        8.0f));

                scene.DrawableObjects.Add(new Sphere(Vector128.Create(-8.5f, 1.0f, 7.0f, 0), new SolidMaterial(Color.Silver), 1.5f));
                scene.DrawableObjects.Add(new Sphere(Vector128.Create(6f, 1.0f, 6.0f, 0),
                                new SolidMaterial(Color.Red, refractivity: .3333f, reflectivity: .13f),
                                2.5f));
                scene.DrawableObjects.Add(new Quad(Vector128.Create(0f, 10, 15, 0), new SolidMaterial(Color.White, reflectivity: .75f, opacity: .95f), -Util.ForwardVector, 30f, 28f, 30f));

                scene.Camera = new Camera(Vector128.Create(0f, 2, -3f, 0), Vector128.Create(0f, -.3f, 1.0f, 0), Util.UpVector, 90f, new Size(1920, 1080));
                return scene;
            }
        }

        public static Scene TwoPlanes
        {
            get
            {
                var scene = new Scene(Color.Sky, Color.White, 0.10f);
                var pos = Util.UpVector * Vector128.Create(50f);
                var size = new System.Drawing.Size(1920, 1080);
                scene.Camera = new Camera(Vector128.Create(0f, 2f, -2f, 0), Vector128.Create(0, -.25f, 1f, 0), Util.UpVector, 90f, size);

                scene.DrawableObjects.Add(new InfinitePlane(Vector128.Create(0, -5f, 0, 0), new CheckerboardMaterial(Color.White, Color.Grey, 1.0f, .1f, 0.0f, 2f), Util.UpVector, 10f));
                scene.DrawableObjects.Add(new InfinitePlane(Vector128.Create(0, 12f, 0, 0), new CheckerboardMaterial(Color.White, Color.Grey, 1.0f, .1f, 0.0f, 12f), -Util.UpVector, 7f));

                scene.DrawableObjects.Add(new Sphere(Vector128.Create(-5, -2.5f, 8, 0), new SolidMaterial(Color.Yellow, reflectivity: .2f), 2.0f));
                scene.DrawableObjects.Add(new Sphere(Vector128.Create(3f, 1, 12, 0), new SolidMaterial(Color.Red, reflectivity: .35f, opacity: .8f), 2.0f));
                scene.DrawableObjects.Add(new Sphere(Vector128.Create(3f, -4, 9, 0), new SolidMaterial(new Color(.33f, .8f, 1.0f, 1.0f), reflectivity: .2222f), 1.0f));
                scene.DrawableObjects.Add(new Sphere(Vector128.Create(0f, 0, 6, 0), new SolidMaterial(Color.Grey, reflectivity: 0.0f, refractivity: 1.0f, opacity: 0.15f), 1.0f));

                scene.DrawableObjects.Add(new Disc(Vector128.Create(-25f, 2, 25, 0), new SolidMaterial(Color.Silver, reflectivity: .63f), Vector128.Create(1f, 0, -1f, 0), 5.0f, 2.0f));

                scene.Lights.Add(new Light(Vector128.Create(0f, 5, 12, 0), 150f, Color.White));

                return scene;
            }
        }

        public static Scene ReflectionMadness
        {
            get
            {
                var scene = new Scene(Color.Sky, Color.White, 0.3f);
                var size = new System.Drawing.Size(1920, 1080);
                scene.Camera = new Camera(Vector128.Create(-5f) * Util.ForwardVector, Util.ForwardVector, Util.UpVector, 90, size);

                // Bounding Box Planes
                scene.DrawableObjects.Add(new InfinitePlane(Vector128.Create(0f, -10, 0, 0), new SolidMaterial(Color.Silver, reflectivity: .8f), Util.UpVector, 10.0f));
                scene.DrawableObjects.Add(new InfinitePlane(Vector128.Create(0f, 10, 0, 0), new SolidMaterial(Color.Silver, reflectivity: .8f), -Util.UpVector, 10.0f));
                scene.DrawableObjects.Add(new InfinitePlane(Vector128.Create(-10f, 0, 0, 0), new SolidMaterial(Color.Silver, reflectivity: .8f), Util.RightVector, 10.0f));
                scene.DrawableObjects.Add(new InfinitePlane(Vector128.Create(10f, 0, 0, 0), new SolidMaterial(Color.Silver, reflectivity: .8f), -Util.RightVector, 10.0f));
                scene.DrawableObjects.Add(new InfinitePlane(Vector128.Create(0f, 0, 10, 0), new SolidMaterial(Color.Silver, reflectivity: .8f), -Util.ForwardVector, 10.0f));
                scene.DrawableObjects.Add(new InfinitePlane(Vector128.Create(0f, 0, -10, 0), new SolidMaterial(Color.Silver, reflectivity: .8f), Util.ForwardVector, 10.0f));

                // Spheres
                scene.DrawableObjects.Add(new Sphere(Vector128.Create(0f, 2, 4, 0), new SolidMaterial(Color.Orange), 1.5f));
                scene.DrawableObjects.Add(new Sphere(Vector128.Create(7.5f, -3, 7, 0), new SolidMaterial(Color.Blue, reflectivity: .4f, refractivity: .25f, opacity: .85f), 2.0f));
                scene.DrawableObjects.Add(new Sphere(Vector128.Create(-5f, -3, 6, 0), new SolidMaterial(Color.Green, reflectivity: .2f, refractivity: .125f, opacity: .75f), 2.6f));

                //Frame Quads
                scene.DrawableObjects.Add(new Quad(Vector128.Create(0, 9.5f, 9.999f, 0), new SolidMaterial(Color.DarkGrey), -Util.ForwardVector, 20.0f, 1.0f, 20f));
                scene.DrawableObjects.Add(new Quad(Vector128.Create(0, -9.5f, 9.999f, 0), new SolidMaterial(Color.DarkGrey), -Util.ForwardVector, 20.0f, 1.0f, 20f));
                scene.DrawableObjects.Add(new Quad(Vector128.Create(-9.5f, 0f, 9.999f, 0), new SolidMaterial(Color.DarkGrey), -Util.ForwardVector, 1.0f, 20.0f, 20f));
                scene.DrawableObjects.Add(new Quad(Vector128.Create(9.5f, 0f, 9.999f, 0), new SolidMaterial(Color.DarkGrey), -Util.ForwardVector, 1.0f, 20.0f, 20f));

                scene.DrawableObjects.Add(new Quad(Vector128.Create(-9.999f, -9.5f, 9.5f, 0), new SolidMaterial(Color.DarkGrey), Util.RightVector, 1.0f, 20.0f, 20f));
                scene.DrawableObjects.Add(new Quad(Vector128.Create(-9.999f, 9.5f, 9.5f, 0), new SolidMaterial(Color.DarkGrey), Util.RightVector, 1.0f, 20.0f, 20f));
                scene.DrawableObjects.Add(new Quad(Vector128.Create(-9.999f, 0f, 9.5f, 0), new SolidMaterial(Color.DarkGrey), Util.RightVector, 20.0f, 1.0f, 20f));
                scene.DrawableObjects.Add(new Quad(Vector128.Create(-9.999f, 0f, -9.5f, 0), new SolidMaterial(Color.DarkGrey), Util.RightVector, 20.0f, 1.0f, 20f));

                scene.DrawableObjects.Add(new Quad(Vector128.Create(9.999f, -9.5f, 9.5f, 0), new SolidMaterial(Color.DarkGrey), -Util.RightVector, 1.0f, 20.0f, 20f));
                scene.DrawableObjects.Add(new Quad(Vector128.Create(9.999f, 9.5f, 9.5f, 0), new SolidMaterial(Color.DarkGrey), -Util.RightVector, 1.0f, 20.0f, 20f));
                scene.DrawableObjects.Add(new Quad(Vector128.Create(9.999f, 0f, 9.5f, 0), new SolidMaterial(Color.DarkGrey), -Util.RightVector, 20.0f, 1.0f, 20f));
                scene.DrawableObjects.Add(new Quad(Vector128.Create(9.999f, 0f, -9.5f, 0), new SolidMaterial(Color.DarkGrey), -Util.RightVector, 20.0f, 1.0f, 20f));

                scene.Lights.Add(new Light(Vector128<float>.Zero, 700000.0f, Color.White));

                return scene;
            }
        }
    }
}
