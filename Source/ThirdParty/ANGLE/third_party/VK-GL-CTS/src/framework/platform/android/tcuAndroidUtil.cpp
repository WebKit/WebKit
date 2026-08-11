/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Android utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuAndroidUtil.hpp"

#include "deSTLUtil.hpp"
#include "deMath.h"

#include <vector>

namespace tcu
{
namespace Android
{

using std::string;
using std::vector;

namespace
{

class ScopedJNIEnv
{
public:
    ScopedJNIEnv(JavaVM *vm);
    ~ScopedJNIEnv(void);

    JavaVM *getVM(void) const
    {
        return m_vm;
    }
    JNIEnv *getEnv(void) const
    {
        return m_env;
    }

private:
    JavaVM *const m_vm;
    JNIEnv *m_env;
    bool m_detach;
};

ScopedJNIEnv::ScopedJNIEnv(JavaVM *vm) : m_vm(vm), m_env(nullptr), m_detach(false)
{
    const int getEnvRes = m_vm->GetEnv((void **)&m_env, JNI_VERSION_1_6);

    if (getEnvRes == JNI_EDETACHED)
    {
        if (m_vm->AttachCurrentThread(&m_env, nullptr) != JNI_OK)
            throw std::runtime_error("JNI AttachCurrentThread() failed");

        m_detach = true;
    }
    else if (getEnvRes != JNI_OK)
        throw std::runtime_error("JNI GetEnv() failed");

    DE_ASSERT(m_env);
}

ScopedJNIEnv::~ScopedJNIEnv(void)
{
    if (m_detach)
        m_vm->DetachCurrentThread();
}

class LocalRef
{
public:
    LocalRef(JNIEnv *env, jobject ref);
    ~LocalRef(void);

    jobject operator*(void) const
    {
        return m_ref;
    }
    operator bool(void) const
    {
        return !!m_ref;
    }

private:
    LocalRef(const LocalRef &);
    LocalRef &operator=(const LocalRef &);

    JNIEnv *const m_env;
    const jobject m_ref;
};

LocalRef::LocalRef(JNIEnv *env, jobject ref) : m_env(env), m_ref(ref)
{
}

LocalRef::~LocalRef(void)
{
    if (m_ref)
        m_env->DeleteLocalRef(m_ref);
}

void checkException(JNIEnv *env)
{
    if (env->ExceptionCheck())
    {
        env->ExceptionDescribe();
        env->ExceptionClear();
        throw std::runtime_error("Got JNI exception");
    }
}

jclass findClass(JNIEnv *env, const char *className)
{
    const jclass cls = env->FindClass(className);

    checkException(env);
    TCU_CHECK_INTERNAL(cls);

    return cls;
}

jclass getObjectClass(JNIEnv *env, jobject object)
{
    const jclass cls = env->GetObjectClass(object);

    checkException(env);
    TCU_CHECK_INTERNAL(cls);

    return cls;
}

jmethodID getMethodID(JNIEnv *env, jclass cls, const char *methodName, const char *signature)
{
    const jmethodID id = env->GetMethodID(cls, methodName, signature);

    checkException(env);
    TCU_CHECK_INTERNAL(id);

    return id;
}

string getStringValue(JNIEnv *env, jstring jniStr)
{
    const char *ptr  = env->GetStringUTFChars(jniStr, nullptr);
    const string str = string(ptr);

    env->ReleaseStringUTFChars(jniStr, ptr);

    return str;
}

string getIntentStringExtra(JNIEnv *env, jobject activity, const char *name)
{
    // \todo [2013-05-12 pyry] Clean up references on error.

    const jclass activityCls = getObjectClass(env, activity);
    const LocalRef intent(
        env, env->CallObjectMethod(activity, getMethodID(env, activityCls, "getIntent", "()Landroid/content/Intent;")));
    TCU_CHECK_INTERNAL(intent);

    const LocalRef extraName(env, env->NewStringUTF(name));
    const jclass intentCls = getObjectClass(env, *intent);
    TCU_CHECK_INTERNAL(extraName && intentCls);

    jvalue getExtraArgs[1];
    getExtraArgs[0].l = *extraName;

    const LocalRef extraStr(env, env->CallObjectMethodA(*intent,
                                                        getMethodID(env, intentCls, "getStringExtra",
                                                                    "(Ljava/lang/String;)Ljava/lang/String;"),
                                                        getExtraArgs));

    if (extraStr)
        return getStringValue(env, (jstring)*extraStr);
    else
        return string();
}

void setRequestedOrientation(JNIEnv *env, jobject activity, ScreenOrientation orientation)
{
    const jclass activityCls         = getObjectClass(env, activity);
    const jmethodID setOrientationId = getMethodID(env, activityCls, "setRequestedOrientation", "(I)V");

    env->CallVoidMethod(activity, setOrientationId, (int)orientation);
}

template <typename Type>
const char *getJNITypeStr(void);

template <>
const char *getJNITypeStr<int>(void)
{
    return "I";
}

template <>
const char *getJNITypeStr<int64_t>(void)
{
    return "J";
}

template <>
const char *getJNITypeStr<string>(void)
{
    return "Ljava/lang/String;";
}

template <>
const char *getJNITypeStr<vector<string>>(void)
{
    return "[Ljava/lang/String;";
}

template <typename FieldType>
FieldType getStaticFieldValue(JNIEnv *env, jclass cls, jfieldID fieldId);

template <>
int getStaticFieldValue<int>(JNIEnv *env, jclass cls, jfieldID fieldId)
{
    DE_ASSERT(cls && fieldId);
    return env->GetStaticIntField(cls, fieldId);
}

template <>
string getStaticFieldValue<string>(JNIEnv *env, jclass cls, jfieldID fieldId)
{
    const jstring jniStr = (jstring)env->GetStaticObjectField(cls, fieldId);

    if (jniStr)
        return getStringValue(env, jniStr);
    else
        return string();
}

template <>
vector<string> getStaticFieldValue<vector<string>>(JNIEnv *env, jclass cls, jfieldID fieldId)
{
    const jobjectArray array = (jobjectArray)env->GetStaticObjectField(cls, fieldId);
    vector<string> result;

    checkException(env);

    if (array)
    {
        const int numElements = env->GetArrayLength(array);

        for (int ndx = 0; ndx < numElements; ndx++)
        {
            const jstring jniStr = (jstring)env->GetObjectArrayElement(array, ndx);

            checkException(env);

            if (jniStr)
                result.push_back(getStringValue(env, jniStr));
        }
    }

    return result;
}

template <typename FieldType>
FieldType getStaticField(JNIEnv *env, const char *className, const char *fieldName)
{
    const jclass cls       = findClass(env, className);
    const jfieldID fieldId = env->GetStaticFieldID(cls, fieldName, getJNITypeStr<FieldType>());

    checkException(env);

    if (fieldId)
        return getStaticFieldValue<FieldType>(env, cls, fieldId);
    else
        throw std::runtime_error(string(fieldName) + " not found in " + className);
}

template <typename FieldType>
FieldType getFieldValue(JNIEnv *env, jobject obj, jfieldID fieldId);

template <>
int64_t getFieldValue<int64_t>(JNIEnv *env, jobject obj, jfieldID fieldId)
{
    DE_ASSERT(obj && fieldId);
    return env->GetLongField(obj, fieldId);
}

template <typename FieldType>
FieldType getField(JNIEnv *env, jobject obj, const char *fieldName)
{
    const jclass cls       = getObjectClass(env, obj);
    const jfieldID fieldId = env->GetFieldID(cls, fieldName, getJNITypeStr<FieldType>());

    checkException(env);

    if (fieldId)
        return getFieldValue<FieldType>(env, obj, fieldId);
    else
        throw std::runtime_error(string(fieldName) + " not found in object");
}

void describePlatform(JNIEnv *env, std::ostream &dst)
{
    const char *const buildClass   = "android/os/Build";
    const char *const versionClass = "android/os/Build$VERSION";

    static const struct
    {
        const char *classPath;
        const char *className;
        const char *fieldName;
    } s_stringFields[] = {
        {buildClass, "Build", "BOARD"},        {buildClass, "Build", "BRAND"},
        {buildClass, "Build", "DEVICE"},       {buildClass, "Build", "DISPLAY"},
        {buildClass, "Build", "FINGERPRINT"},  {buildClass, "Build", "HARDWARE"},
        {buildClass, "Build", "MANUFACTURER"}, {buildClass, "Build", "MODEL"},
        {buildClass, "Build", "PRODUCT"},      {buildClass, "Build", "TAGS"},
        {buildClass, "Build", "TYPE"},         {versionClass, "Build.VERSION", "RELEASE"},
    };

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_stringFields); ndx++)
        dst << s_stringFields[ndx].className << "." << s_stringFields[ndx].fieldName << ": "
            << getStaticField<string>(env, s_stringFields[ndx].classPath, s_stringFields[ndx].fieldName) << "\n";

    dst << "Build.VERSION.SDK_INT: " << getStaticField<int>(env, versionClass, "SDK_INT") << "\n";

    {
        const vector<string> supportedAbis = getStaticField<vector<string>>(env, buildClass, "SUPPORTED_ABIS");

        dst << "Build.SUPPORTED_ABIS: ";

        for (size_t ndx = 0; ndx < supportedAbis.size(); ndx++)
            dst << (ndx != 0 ? ", " : "") << supportedAbis[ndx];

        dst << "\n";
    }
}

} // namespace

ScreenOrientation mapScreenRotation(ScreenRotation rotation)
{
    switch (rotation)
    {
    case SCREENROTATION_UNSPECIFIED:
        return SCREEN_ORIENTATION_UNSPECIFIED;
    case SCREENROTATION_0:
        return SCREEN_ORIENTATION_PORTRAIT;
    case SCREENROTATION_90:
        return SCREEN_ORIENTATION_LANDSCAPE;
    case SCREENROTATION_180:
        return SCREEN_ORIENTATION_REVERSE_PORTRAIT;
    case SCREENROTATION_270:
        return SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
    default:
        print("Warning: Unsupported rotation");
        return SCREEN_ORIENTATION_PORTRAIT;
    }
}

static jclass loadClassWithActivityClassLoader(JNIEnv *env, jobject activity, const char *path)
{
    jclass activityCls = env->GetObjectClass(activity);
    checkException(env);
    jmethodID getClassLoader = env->GetMethodID(activityCls, "getClassLoader", "()Ljava/lang/ClassLoader;");
    checkException(env);

    jobject classLoaderObj = env->CallObjectMethod(activity, getClassLoader);
    checkException(env);

    jclass classLoaderCls = env->GetObjectClass(classLoaderObj);
    checkException(env);
    jmethodID loadClass = env->GetMethodID(classLoaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    checkException(env);

    jstring jname = env->NewStringUTF(path);
    checkException(env);

    jclass result = (jclass)env->CallObjectMethod(classLoaderObj, loadClass, jname);
    checkException(env);

    env->DeleteLocalRef(jname);
    env->DeleteLocalRef(classLoaderCls);
    env->DeleteLocalRef(classLoaderObj);
    env->DeleteLocalRef(activityCls);

    TCU_CHECK_INTERNAL(result);
    return result;
}

void PixelCopy(ANativeActivity *activity, const char *path)
{
    const ScopedJNIEnv scopedEnv(activity->vm);
    JNIEnv *env = scopedEnv.getEnv();

    jclass cls = loadClassWithActivityClassLoader(env, activity->clazz,
                                                  "com.drawelements.deqp.platformutil.DeqpPlatformRequestPixelCopy");

    jmethodID mid = env->GetStaticMethodID(cls, "requestPixelCopyToPng", "(Landroid/app/Activity;Ljava/lang/String;)V");
    checkException(env);
    TCU_CHECK_INTERNAL(mid);

    const LocalRef jpath(env, env->NewStringUTF(path));
    checkException(env);
    TCU_CHECK_INTERNAL(*jpath);

    env->CallStaticVoidMethod(cls, mid, activity->clazz, *jpath);
    checkException(env);

    env->DeleteLocalRef(cls);
}

string getIntentStringExtra(ANativeActivity *activity, const char *name)
{
    const ScopedJNIEnv env(activity->vm);

    return getIntentStringExtra(env.getEnv(), activity->clazz, name);
}

void setRequestedOrientation(ANativeActivity *activity, ScreenOrientation orientation)
{
    const ScopedJNIEnv env(activity->vm);

    setRequestedOrientation(env.getEnv(), activity->clazz, orientation);
}

void describePlatform(ANativeActivity *activity, std::ostream &dst)
{
    const ScopedJNIEnv env(activity->vm);

    describePlatform(env.getEnv(), dst);
}

size_t getTotalAndroidSystemMemory(ANativeActivity *activity)
{
    const ScopedJNIEnv scopedJniEnv(activity->vm);
    JNIEnv *env = scopedJniEnv.getEnv();

    // Get activity manager instance:
    // ActivityManager activityManager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
    const jclass activityManagerClass = findClass(env, "android/app/ActivityManager");
    const LocalRef activityString(env, env->NewStringUTF("activity")); // Context.ACTIVITY_SERVICE == "activity"
    const jclass activityClass = getObjectClass(env, activity->clazz);
    const jmethodID getServiceID =
        getMethodID(env, activityClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    LocalRef activityManager(env, env->CallObjectMethod(activity->clazz, getServiceID, *activityString));
    checkException(env);
    TCU_CHECK_INTERNAL(activityManager);

    // Crete memory info instance:
    // ActivityManager.MemoryInfo memoryInfo = new ActivityManager.MemoryInfo();
    const jclass memoryInfoClass   = findClass(env, "android/app/ActivityManager$MemoryInfo");
    const jmethodID memoryInfoCtor = getMethodID(env, memoryInfoClass, "<init>", "()V");
    LocalRef memoryInfo(env, env->NewObject(memoryInfoClass, memoryInfoCtor));
    checkException(env);
    TCU_CHECK_INTERNAL(memoryInfo);

    // Get memory info from activity manager:
    // activityManager.getMemoryInfo(memoryInfo);
    const jmethodID getMemoryInfoID =
        getMethodID(env, activityManagerClass, "getMemoryInfo", "(Landroid/app/ActivityManager$MemoryInfo;)V");
    checkException(env);
    env->CallVoidMethod(*activityManager, getMemoryInfoID, *memoryInfo);

    // Return 'totalMem' field from the memory info instance.
    return static_cast<size_t>(getField<int64_t>(env, *memoryInfo, "totalMem"));
}

} // namespace Android
} // namespace tcu
