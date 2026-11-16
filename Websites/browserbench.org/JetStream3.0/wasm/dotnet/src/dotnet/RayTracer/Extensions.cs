using System;
using System.Runtime.CompilerServices;
using System.Runtime.Intrinsics;

namespace RayTracer
{
    public static class Extensions
    {
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        static public float DotR(this Vector128<float> left, Vector128<float> right)
        {
            var vm = left * right;
            return vm.X() + vm.Y() + vm.Z();
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        static public float Magnitude(this Vector128<float> v)
        {
            return (float)Math.Sqrt(v.DotR(v));
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        static public Vector128<float> Normalize(this Vector128<float> v)
        {
            return v / Vector128.Create(v.Magnitude());
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        static public float X(this Vector128<float> v)
        {
            return v.GetElement(0);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        static public float Y(this Vector128<float> v)
        {
            return v.GetElement(1);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        static public float Z(this Vector128<float> v)
        {
            return v.GetElement(2);
        }
    }
}
