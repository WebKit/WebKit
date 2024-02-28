# Proguard rules for the JNI instrumentation tests.

# These classes are needed by the instrumentation testing framework. Keep them
# unobfuscated.
-keep class org.aomedia.avif.android.AvifDecoderTest {
  *;
}
