#include "jni_utility.h"


static bool attachToJavaVM(JavaVM **jvm, JNIEnv **env)
{
    JavaVM *jvmArray[1];
    jsize bufLen = 1;
    jsize nJVMs = 0;
    jint jniError = 0;
    bool attached = false;

    // if a JVM is already running ...
    jniError = JNI_GetCreatedJavaVMs(jvmArray, bufLen, &nJVMs);
    if ( jniError == JNI_OK )
    {
        // how many JVMs?
        if ( nJVMs > 0 )
        {
            // connect to the Java VM
            // there can be only one per process  -- mhay  2002.09.30
            *jvm = jvmArray[0];
            jniError = (*jvm)->AttachCurrentThread((void**)env, (void *)NULL);
            if ( jniError == JNI_OK )
                attached = true;
            else
                fprintf(stderr, "%s: AttachCurrentThread failed, returned %d", __PRETTY_FUNCTION__, jniError);
        }
    }
    else 
        fprintf(stderr, "%s: JNI_GetCreatedJavaVMs failed, returned %d", __PRETTY_FUNCTION__, jniError);

    return attached;
}

typedef union {
    jobject _object;
    jboolean _boolean;
    jbyte _byte;
    jchar _char;
    jshort _short;
    jint _int;
    jlong _long;
    jfloat _float;
    jdouble _double;
    bool _error;
} jresult;

typedef enum {
    void_function,
    object_function,
    boolean_function,
    byte_function,
    char_function,
    short_function,
    int_function,
    long_function,
    float_function,
    double_function
} JNIFunctionType;


jresult callJNIMethod( JNIFunctionType type, jobject obj, const char *name, const char *sig, va_list argList)
{
    JavaVM *jvm = NULL;
    JNIEnv *env = NULL;
    jresult result;

    result._error = false;
    
    if ( obj != NULL ) {
        if ( attachToJavaVM(&jvm, &env) ) {
            jclass cls = env->GetObjectClass(obj);
            if ( cls != NULL ) {
                jmethodID mid = env->GetMethodID(cls, name, sig);
                if ( mid != NULL )
                {
                    switch (type) {
                    case void_function:
                        env->functions->CallVoidMethodV(env, obj, mid, argList);
                        break;
                    case object_function:
                        result._object = env->functions->CallObjectMethodV(env, obj, mid, argList);
                        break;
                    case boolean_function:
                        result._boolean = env->functions->CallBooleanMethodV(env, obj, mid, argList);
                        break;
                    case byte_function:
                        result._byte = env->functions->CallByteMethodV(env, obj, mid, argList);
                        break;
                    case char_function:
                        result._char = env->functions->CallCharMethodV(env, obj, mid, argList);
                        break;
                    case short_function:
                        result._short = env->functions->CallShortMethodV(env, obj, mid, argList);
                        break;
                    case int_function:
                        result._int = env->functions->CallIntMethodV(env, obj, mid, argList);
                        break;
                    case long_function:
                        result._long = env->functions->CallLongMethodV(env, obj, mid, argList);
                        break;
                    case float_function:
                        result._float = env->functions->CallFloatMethodV(env, obj, mid, argList);
                        break;
                    case double_function:
                        result._double = env->functions->CallDoubleMethodV(env, obj, mid, argList);
                        break;
                    default:
                        result._error = true;
                    }
                }
                else
                {
                    fprintf(stderr, "%s: Could not find method: %s!", __PRETTY_FUNCTION__, name);
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                    result._error = true;
                }

                env->DeleteLocalRef(cls);
            }
            else {
                fprintf(stderr, "%s: Could not find class for object!", __PRETTY_FUNCTION__);
                result._error = true;
            }
        }
        else {
            fprintf(stderr, "%s: Could not attach to the VM!", __PRETTY_FUNCTION__);
            result._error = true;
        }
    }

    return result;
}

#define CALL_JNI_METHOD(function_type,obj,name,sig) \
    va_list args;\
    va_start (args, sig);\
    \
    jresult result = callJNIMethod(function_type, obj, name, sig, args);\
    \
    va_end (args);

void callJNIVoidMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (void_function, obj, name, sig);
}

jobject callJNIObjectMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (object_function, obj, name, sig);
    return result._object;
}

jbyte callJNIByteMethod( jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (byte_function, obj, name, sig);
    return result._byte;
}

jchar callJNICharMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (char_function, obj, name, sig);
    return result._char;
}

jshort callJNIShortMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (short_function, obj, name, sig);
    return result._short;
}

jint callJNIIntMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (int_function, obj, name, sig);
    return result._int;
}

jlong callJNILongMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (long_function, obj, name, sig);
    return result._long;
}

jfloat callJNIFloatMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (float_function, obj, name, sig);
    return result._float;
}

jdouble callJNIDoubleMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (double_function, obj, name, sig);
    return result._double;
}

