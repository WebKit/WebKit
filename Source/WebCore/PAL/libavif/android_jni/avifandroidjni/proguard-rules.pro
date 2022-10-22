# Proguard rules for the JNI Bindings.
# For explanation, see:
# https://www.guardsquare.com/manual/configuration/examples#native

# Keep the names of the native methods unobfuscated.
-keepclasseswithmembernames,includedescriptorclasses class * {
  native <methods>;
}

# Members of this class may be accessed from native methods. Keep them
# unobfuscated.
-keep class org.aomedia.avif.android.AvifDecoder$Info {
  *;
}
