#include <JavaVM/jni.h>

jobject callJNIObjectMethod( jobject obj, const char *name, const char *sig, ... );
void callJNIVoidMethod( jobject obj, const char *name, const char *sig, ... );
jboolean callJNIBooleanMethod( jobject obj, const char *name, const char *sig, ... );
jbyte callJNIByteMethod( jobject obj, const char *name, const char *sig, ... );
jchar callJNICharMethod( jobject obj, const char *name, const char *sig, ... );
jshort callJNIShortMethod( jobject obj, const char *name, const char *sig, ... );
jint callJNIIntMethod( jobject obj, const char *name, const char *sig, ... );
jlong callJNILongMethod( jobject obj, const char *name, const char *sig, ... );
jfloat callJNIFloatMethod( jobject obj, const char *name, const char *sig, ... );
jdouble callJNIDoubleMethod( jobject obj, const char *name, const char *sig, ... );
