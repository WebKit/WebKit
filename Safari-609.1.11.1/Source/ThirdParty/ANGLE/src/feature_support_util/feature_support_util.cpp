//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// feature_support_util.cpp: Helps client APIs make decisions based on rules
// data files.  For example, the Android EGL loader uses this library to
// determine whether to use ANGLE or a native GLES driver.

#include "feature_support_util.h"
#include <json/json.h>
#include <string.h>
#include "common/platform.h"
#if defined(ANGLE_PLATFORM_ANDROID)
#    include <android/log.h>
#    include <unistd.h>
#endif
#include <fstream>
#include <list>
#include "../gpu_info_util/SystemInfo.h"

namespace angle
{

#if defined(ANGLE_PLATFORM_ANDROID)
// Define ANGLE_FEATURE_UTIL_LOG_VERBOSE if you want VERBOSE to output
// ANGLE_FEATURE_UTIL_LOG_VERBOSE is automatically defined when is_debug = true

#    define ERR(...) __android_log_print(ANDROID_LOG_ERROR, "ANGLE", __VA_ARGS__)
#    define WARN(...) __android_log_print(ANDROID_LOG_WARN, "ANGLE", __VA_ARGS__)
#    define INFO(...) __android_log_print(ANDROID_LOG_INFO, "ANGLE", __VA_ARGS__)
#    define DEBUG(...) __android_log_print(ANDROID_LOG_DEBUG, "ANGLE", __VA_ARGS__)
#    ifdef ANGLE_FEATURE_UTIL_LOG_VERBOSE
#        define VERBOSE(...) __android_log_print(ANDROID_LOG_VERBOSE, "ANGLE", __VA_ARGS__)
#    else
#        define VERBOSE(...) ((void)0)
#    endif
#else  // defined(ANDROID)
#    define ERR(...) printf(__VA_ARGS__)
#    define WARN(...) printf(__VA_ARGS__)
#    define INFO(...) printf(__VA_ARGS__)
#    define DEBUG(...) printf(__VA_ARGS__)
// Uncomment for debugging.
//#    define VERBOSE(...) printf(__VA_ARGS__)
#    define VERBOSE(...)
#endif  // defined(ANDROID)

// JSON values are generally composed of either:
//  - Objects, which are a set of comma-separated string:value pairs (note the recursive nature)
//  - Arrays, which are a set of comma-separated values.
// We'll call the string in a string:value pair the "identifier".  These identifiers are defined
// below, as follows:

// The JSON identifier for the top-level set of rules.  This is an object, the value of which is an
// array of rules.  The rules will be processed in order.  If a rule matches, the rule's version of
// the answer (true or false) becomes the new answer.  After all rules are processed, the
// most-recent answer is the final answer.
constexpr char kJsonRules[] = "Rules";
// The JSON identifier for a given rule.  A rule is an object, the first string:value pair is this
// identifier (i.e. "Rule") as the string and the value is a user-firendly description of the rule:
constexpr char kJsonRule[] = "Rule";
// Within a rule, the JSON identifier for the answer--whether or not to use ANGLE.  The value is a
// boolean (i.e. true or false).
constexpr char kJsonUseANGLE[] = "UseANGLE";

// Within a rule, the JSON identifier for describing one or more applications.  The value is an
// array of objects, each object of which can specify attributes of an application.
constexpr char kJsonApplications[] = "Applications";
// Within an object that describes the attributes of an application, the JSON identifier for the
// name of the application (e.g. "com.google.maps").  The value is a string.  If any other
// attributes will be specified, this must be the first attribute specified in the object.
constexpr char kJsonAppName[] = "AppName";

// Within a rule, the JSON identifier for describing one or more devices.  The value is an
// array of objects, each object of which can specify attributes of a device.
constexpr char kJsonDevices[] = "Devices";
// Within an object that describes the attributes of a device, the JSON identifier for the
// manufacturer of the device.  The value is a string.  If any other non-GPU attributes will be
// specified, this must be the first attribute specified in the object.
constexpr char kJsonManufacturer[] = "Manufacturer";
// Within an object that describes the attributes of a device, the JSON identifier for the
// model of the device.  The value is a string.
constexpr char kJsonModel[] = "Model";

// Within an object that describes the attributes of a device, the JSON identifier for describing
// one or more GPUs/drivers used in the device.  The value is an
// array of objects, each object of which can specify attributes of a GPU and its driver.
constexpr char kJsonGPUs[] = "GPUs";
// Within an object that describes the attributes of a GPU and driver, the JSON identifier for the
// vendor of the device/driver.  The value is a string.  If any other attributes will be specified,
// this must be the first attribute specified in the object.
constexpr char kJsonVendor[] = "Vendor";
// Within an object that describes the attributes of a GPU and driver, the JSON identifier for the
// deviceId of the device.  The value is an unsigned integer.  If the driver version will be
// specified, this must preceded the version attributes specified in the object.
constexpr char kJsonDeviceId[] = "DeviceId";

// Within an object that describes the attributes of either an application or a GPU, the JSON
// identifier for the major version of that application or GPU driver.  The value is a positive
// integer number.  Not specifying a major version implies a wildcard for all values of a version.
constexpr char kJsonVerMajor[] = "VerMajor";
// Within an object that describes the attributes of either an application or a GPU, the JSON
// identifier for the minor version of that application or GPU driver.  The value is a positive
// integer number.  In order to specify a minor version, it must be specified immediately after the
// major number associated with it.  Not specifying a minor version implies a wildcard for the
// minor, subminor, and patch values of a version.
constexpr char kJsonVerMinor[] = "VerMinor";
// Within an object that describes the attributes of either an application or a GPU, the JSON
// identifier for the subminor version of that application or GPU driver.  The value is a positive
// integer number.  In order to specify a subminor version, it must be specified immediately after
// the minor number associated with it.  Not specifying a subminor version implies a wildcard for
// the subminor and patch values of a version.
constexpr char kJsonVerSubMinor[] = "VerSubMinor";
// Within an object that describes the attributes of either an application or a GPU, the JSON
// identifier for the patch version of that application or GPU driver.  The value is a positive
// integer number.  In order to specify a patch version, it must be specified immediately after the
// subminor number associated with it.  Not specifying a patch version implies a wildcard for the
// patch value of a version.
constexpr char kJsonVerPatch[] = "VerPatch";

// This encapsulates a std::string.  The default constructor (not given a string) assumes that this
// is a wildcard (i.e. will match all other StringPart objects).
class StringPart
{
  public:
    StringPart() : mPart(""), mWildcard(true) {}
    StringPart(const std::string part) : mPart(part), mWildcard(false) {}
    ~StringPart() {}
    bool match(const StringPart &toCheck) const
    {
        return (mWildcard || toCheck.mWildcard || (toCheck.mPart == mPart));
    }

  public:
    std::string mPart;
    bool mWildcard;
};

// This encapsulates a 32-bit unsigned integer.  The default constructor (not given a number)
// assumes that this is a wildcard (i.e. will match all other IntegerPart objects).
class IntegerPart
{
  public:
    IntegerPart() : mPart(0), mWildcard(true) {}
    IntegerPart(uint32_t part) : mPart(part), mWildcard(false) {}
    ~IntegerPart() {}
    bool match(const IntegerPart &toCheck) const
    {
        return (mWildcard || toCheck.mWildcard || (toCheck.mPart == mPart));
    }

  public:
    uint32_t mPart;
    bool mWildcard;
};

// This encapsulates a list of other classes, each of which will have a match() and logItem()
// method.  The common constructor (given a type, but not any list items) assumes that this is
// a wildcard (i.e. will match all other ListOf<t> objects).
template <class T>
class ListOf
{
  public:
    ListOf(const std::string listType) : mWildcard(true), mListType(listType) {}
    ~ListOf() { mList.clear(); }
    void addItem(const T &toAdd)
    {
        mList.push_back(toAdd);
        mWildcard = false;
    }
    bool match(const T &toCheck) const
    {
        VERBOSE("\t\t Within ListOf<%s> match: wildcards are %s and %s,\n", mListType.c_str(),
                mWildcard ? "true" : "false", toCheck.mWildcard ? "true" : "false");
        if (mWildcard || toCheck.mWildcard)
        {
            return true;
        }
        for (const T &it : mList)
        {
            VERBOSE("\t\t   Within ListOf<%s> match: calling match on sub-item is %s,\n",
                    mListType.c_str(), it.match(toCheck) ? "true" : "false");
            if (it.match(toCheck))
            {
                return true;
            }
        }
        return false;
    }
    bool match(const ListOf<T> &toCheck) const
    {
        VERBOSE("\t\t Within ListOf<%s> match: wildcards are %s and %s,\n", mListType.c_str(),
                mWildcard ? "true" : "false", toCheck.mWildcard ? "true" : "false");
        if (mWildcard || toCheck.mWildcard)
        {
            return true;
        }
        // If we make it to here, both this and toCheck have at least one item in their mList
        for (const T &it : toCheck.mList)
        {
            if (match(it))
            {
                return true;
            }
        }
        return false;
    }
    void logListOf(const std::string prefix, const std::string name) const
    {
        if (mWildcard)
        {
            VERBOSE("%sListOf%s is wildcarded to always match", prefix.c_str(), name.c_str());
        }
        else
        {
            VERBOSE("%sListOf%s has %d item(s):", prefix.c_str(), name.c_str(),
                    static_cast<int>(mList.size()));
            for (auto &it : mList)
            {
                it.logItem();
            }
        }
    }

    bool mWildcard;

  private:
    std::string mListType;
    std::vector<T> mList;
};

// This encapsulates up-to four 32-bit unsigned integers, that represent a potentially-complex
// version number.  The default constructor (not given any numbers) assumes that this is a wildcard
// (i.e. will match all other Version objects).  Each part of a Version is stored in an IntegerPart
// class, and so may be wildcarded as well.
class Version
{
  public:
    Version(uint32_t major, uint32_t minor, uint32_t subminor, uint32_t patch)
        : mMajor(major), mMinor(minor), mSubminor(subminor), mPatch(patch), mWildcard(false)
    {}
    Version(uint32_t major, uint32_t minor, uint32_t subminor)
        : mMajor(major), mMinor(minor), mSubminor(subminor), mWildcard(false)
    {}
    Version(uint32_t major, uint32_t minor) : mMajor(major), mMinor(minor), mWildcard(false) {}
    Version(uint32_t major) : mMajor(major), mWildcard(false) {}
    Version() : mWildcard(true) {}
    Version(const Version &toCopy)
        : mMajor(toCopy.mMajor),
          mMinor(toCopy.mMinor),
          mSubminor(toCopy.mSubminor),
          mPatch(toCopy.mPatch),
          mWildcard(toCopy.mWildcard)
    {}
    ~Version() {}

    static Version *CreateVersionFromJson(const Json::Value &jObject)
    {
        Version *version = nullptr;
        // A major version must be provided before a minor, and so on:
        if (jObject.isMember(kJsonVerMajor) && jObject[kJsonVerMajor].isInt())
        {
            int major = jObject[kJsonVerMajor].asInt();
            if (jObject.isMember(kJsonVerMinor) && jObject[kJsonVerMinor].isInt())
            {
                int minor = jObject[kJsonVerMinor].asInt();
                if (jObject.isMember(kJsonVerSubMinor) && jObject[kJsonVerSubMinor].isInt())
                {
                    int subMinor = jObject[kJsonVerSubMinor].asInt();
                    if (jObject.isMember(kJsonVerPatch) && jObject[kJsonVerPatch].isInt())
                    {
                        int patch = jObject[kJsonVerPatch].asInt();
                        version   = new Version(major, minor, subMinor, patch);
                    }
                    else
                    {
                        version = new Version(major, minor, subMinor);
                    }
                }
                else
                {
                    version = new Version(major, minor);
                }
            }
            else
            {
                version = new Version(major);
            }
        }
        return version;
    }

    bool match(const Version &toCheck) const
    {
        VERBOSE("\t\t\t Within Version %d,%d,%d,%d match(%d,%d,%d,%d): wildcards are %s and %s,\n",
                mMajor.mPart, mMinor.mPart, mSubminor.mPart, mPatch.mPart, toCheck.mMajor.mPart,
                toCheck.mMinor.mPart, toCheck.mSubminor.mPart, toCheck.mPatch.mPart,
                mWildcard ? "true" : "false", toCheck.mWildcard ? "true" : "false");
        if (!(mWildcard || toCheck.mWildcard))
        {
            VERBOSE("\t\t\t   mMajor match is %s, mMinor is %s, mSubminor is %s, mPatch is %s\n",
                    mMajor.match(toCheck.mMajor) ? "true" : "false",
                    mMinor.match(toCheck.mMinor) ? "true" : "false",
                    mSubminor.match(toCheck.mSubminor) ? "true" : "false",
                    mPatch.match(toCheck.mPatch) ? "true" : "false");
        }
        return (mWildcard || toCheck.mWildcard ||
                (mMajor.match(toCheck.mMajor) && mMinor.match(toCheck.mMinor) &&
                 mSubminor.match(toCheck.mSubminor) && mPatch.match(toCheck.mPatch)));
    }
    std::string getString() const
    {
        if (mWildcard)
        {
            return "*";
        }
        else
        {
            char ret[100];
            // Must at least have a major version:
            if (!mMinor.mWildcard)
            {
                if (!mSubminor.mWildcard)
                {
                    if (!mPatch.mWildcard)
                    {
                        snprintf(ret, 100, "%d.%d.%d.%d", mMajor.mPart, mMinor.mPart,
                                 mSubminor.mPart, mPatch.mPart);
                    }
                    else
                    {
                        snprintf(ret, 100, "%d.%d.%d.*", mMajor.mPart, mMinor.mPart,
                                 mSubminor.mPart);
                    }
                }
                else
                {
                    snprintf(ret, 100, "%d.%d.*", mMajor.mPart, mMinor.mPart);
                }
            }
            else
            {
                snprintf(ret, 100, "%d.*", mMajor.mPart);
            }
            std::string retString = ret;
            return retString;
        }
    }

  public:
    IntegerPart mMajor;
    IntegerPart mMinor;
    IntegerPart mSubminor;
    IntegerPart mPatch;
    bool mWildcard;
};

// This encapsulates an application, and potentially the application's Version.  The default
// constructor (not given any values) assumes that this is a wildcard (i.e. will match all
// other Application objects).  Each part of an Application is stored in a class that may
// also be wildcarded.
class Application
{
  public:
    Application(const std::string name, const Version &version)
        : mName(name), mVersion(version), mWildcard(false)
    {}
    Application(const std::string name) : mName(name), mVersion(), mWildcard(false) {}
    Application() : mName(), mVersion(), mWildcard(true) {}
    ~Application() {}

    static Application *CreateApplicationFromJson(const Json::Value &jObject)
    {
        Application *application = nullptr;

        // If an application is listed, the application's name is required:
        std::string appName = jObject[kJsonAppName].asString();

        // The application's version is optional:
        Version *version = Version::CreateVersionFromJson(jObject);
        if (version)
        {
            application = new Application(appName, *version);
            delete version;
        }
        else
        {
            application = new Application(appName);
        }
        return application;
    }

    bool match(const Application &toCheck) const
    {
        return (mWildcard || toCheck.mWildcard ||
                (toCheck.mName.match(mName) && toCheck.mVersion.match(mVersion)));
    }
    void logItem() const
    {
        if (mWildcard)
        {
            VERBOSE("      Wildcard (i.e. will match all applications)");
        }
        else if (!mVersion.mWildcard)
        {
            VERBOSE("      Application \"%s\" (version: %s)", mName.mPart.c_str(),
                    mVersion.getString().c_str());
        }
        else
        {
            VERBOSE("      Application \"%s\"", mName.mPart.c_str());
        }
    }

  public:
    StringPart mName;
    Version mVersion;
    bool mWildcard;
};

// This encapsulates a GPU and its driver.  The default constructor (not given any values) assumes
// that this is a wildcard (i.e. will match all other GPU objects).  Each part of a GPU is stored
// in a class that may also be wildcarded.
class GPU
{
  public:
    GPU(const std::string vendor, uint32_t deviceId, const Version &version)
        : mVendor(vendor), mDeviceId(IntegerPart(deviceId)), mVersion(version), mWildcard(false)
    {}
    GPU(const std::string vendor, uint32_t deviceId)
        : mVendor(vendor), mDeviceId(IntegerPart(deviceId)), mVersion(), mWildcard(false)
    {}
    GPU(const std::string vendor) : mVendor(vendor), mDeviceId(), mVersion(), mWildcard(false) {}
    GPU() : mVendor(), mDeviceId(), mVersion(), mWildcard(true) {}
    bool match(const GPU &toCheck) const
    {
        VERBOSE("\t\t Within GPU match: wildcards are %s and %s,\n", mWildcard ? "true" : "false",
                toCheck.mWildcard ? "true" : "false");
        VERBOSE("\t\t   mVendor = \"%s\" and toCheck.mVendor = \"%s\"\n", mVendor.mPart.c_str(),
                toCheck.mVendor.mPart.c_str());
        VERBOSE("\t\t   mDeviceId = %d and toCheck.mDeviceId = %d\n", mDeviceId.mPart,
                toCheck.mDeviceId.mPart);
        VERBOSE("\t\t   mVendor match is %s, mDeviceId is %s, mVersion is %s\n",
                toCheck.mVendor.match(mVendor) ? "true" : "false",
                toCheck.mDeviceId.match(mDeviceId) ? "true" : "false",
                toCheck.mVersion.match(mVersion) ? "true" : "false");
        return (mWildcard || toCheck.mWildcard ||
                (toCheck.mVendor.match(mVendor) && toCheck.mDeviceId.match(mDeviceId) &&
                 toCheck.mVersion.match(mVersion)));
    }
    ~GPU() {}

    static GPU *CreateGpuFromJson(const Json::Value &jObject)
    {
        GPU *gpu = nullptr;

        // If a GPU is listed, the vendor name is required:
        if (jObject.isMember(kJsonVendor) && jObject[kJsonVendor].isString())
        {
            std::string vendor = jObject[kJsonVendor].asString();
            // If a version is given, the deviceId is required:
            if (jObject.isMember(kJsonDeviceId) && jObject[kJsonDeviceId].isUInt())
            {
                uint32_t deviceId = jObject[kJsonDeviceId].asUInt();
                Version *version  = Version::CreateVersionFromJson(jObject);
                if (version)
                {
                    gpu = new GPU(vendor, deviceId, *version);
                    delete version;
                }
                else
                {
                    gpu = new GPU(vendor, deviceId);
                }
            }
            else
            {
                gpu = new GPU(vendor);
            }
        }
        else
        {
            WARN("Asked to parse a GPU, but no vendor found");
        }

        return gpu;
    }

    void logItem() const
    {
        if (mWildcard)
        {
            VERBOSE("          Wildcard (i.e. will match all GPUs)");
        }
        else
        {
            if (!mDeviceId.mWildcard)
            {
                if (!mVersion.mWildcard)
                {
                    VERBOSE("\t     GPU vendor: %s, deviceId: 0x%x, version: %s",
                            mVendor.mPart.c_str(), mDeviceId.mPart, mVersion.getString().c_str());
                }
                else
                {
                    VERBOSE("\t     GPU vendor: %s, deviceId: 0x%x", mVendor.mPart.c_str(),
                            mDeviceId.mPart);
                }
            }
            else
            {
                VERBOSE("\t     GPU vendor: %s", mVendor.mPart.c_str());
            }
        }
    }

  public:
    StringPart mVendor;
    IntegerPart mDeviceId;
    Version mVersion;
    bool mWildcard;
};

// This encapsulates a device, and potentially the device's model and/or a list of GPUs/drivers
// associated with the Device.  The default constructor (not given any values) assumes that this is
// a wildcard (i.e. will match all other Device objects).  Each part of a Device is stored in a
// class that may also be wildcarded.
class Device
{
  public:
    Device(const std::string manufacturer, const std::string model)
        : mManufacturer(manufacturer), mModel(model), mGpuList("GPU"), mWildcard(false)
    {}
    Device(const std::string manufacturer)
        : mManufacturer(manufacturer), mModel(), mGpuList("GPU"), mWildcard(false)
    {}
    Device() : mManufacturer(), mModel(), mGpuList("GPU"), mWildcard(true) {}
    ~Device() {}

    static Device *CreateDeviceFromJson(const Json::Value &jObject)
    {
        Device *device = nullptr;
        if (jObject.isMember(kJsonManufacturer) && jObject[kJsonManufacturer].isString())
        {
            std::string manufacturerName = jObject[kJsonManufacturer].asString();
            // We don't let a model be specified without also specifying an Manufacturer:
            if (jObject.isMember(kJsonModel) && jObject[kJsonModel].isString())
            {
                std::string model = jObject[kJsonModel].asString();
                device            = new Device(manufacturerName, model);
            }
            else
            {
                device = new Device(manufacturerName);
            }
        }
        else
        {
            // This case is not treated as an error because a rule may wish to only call out one or
            // more GPUs, but not any specific Manufacturer (e.g. for any manufacturer's device
            // that uses a GPU from Vendor-A, with DeviceID-Foo, and with driver version 1.2.3.4):
            device = new Device();
        }
        return device;
    }

    void addGPU(const GPU &gpu) { mGpuList.addItem(gpu); }
    bool match(const Device &toCheck) const
    {
        VERBOSE("\t Within Device match: wildcards are %s and %s,\n", mWildcard ? "true" : "false",
                toCheck.mWildcard ? "true" : "false");
        if (!(mWildcard || toCheck.mWildcard))
        {
            VERBOSE("\t   Manufacturer match is %s, model is %s\n",
                    toCheck.mManufacturer.match(mManufacturer) ? "true" : "false",
                    toCheck.mModel.match(mModel) ? "true" : "false");
        }
        VERBOSE("\t   Need to check ListOf<GPU>\n");
        return ((mWildcard || toCheck.mWildcard ||
                 // The wildcards can override the Manufacturer/Model check, but not the GPU check
                 (toCheck.mManufacturer.match(mManufacturer) && toCheck.mModel.match(mModel))) &&
                mGpuList.match(toCheck.mGpuList));
    }
    void logItem() const
    {
        if (mWildcard)
        {
            if (mGpuList.mWildcard)
            {
                VERBOSE("      Wildcard (i.e. will match all devices)");
                return;
            }
            else
            {
                VERBOSE(
                    "      Device with any manufacturer and model"
                    ", and with the following GPUs:");
            }
        }
        else
        {
            if (!mModel.mWildcard)
            {
                VERBOSE(
                    "      Device manufacturer: \"%s\" and model \"%s\""
                    ", and with the following GPUs:",
                    mManufacturer.mPart.c_str(), mModel.mPart.c_str());
            }
            else
            {
                VERBOSE(
                    "      Device manufacturer: \"%s\""
                    ", and with the following GPUs:",
                    mManufacturer.mPart.c_str());
            }
        }
        mGpuList.logListOf("        ", "GPUs");
    }

  public:
    StringPart mManufacturer;
    StringPart mModel;
    ListOf<GPU> mGpuList;
    bool mWildcard;
};

// This encapsulates a particular scenario to check against the rules.  A Scenario is similar to a
// Rule, except that a Rule has an answer and potentially many wildcards, and a Scenario is the
// fully-specified combination of an Application and a Device that is proposed to be run with
// ANGLE.  It is compared with the list of Rules.
class Scenario
{
  public:
    Scenario(const char *appName, const char *deviceMfr, const char *deviceModel)
        : mApplication(Application(appName)), mDevice(Device(deviceMfr, deviceModel))
    {}
    ~Scenario() {}
    void logScenario()
    {
        VERBOSE("  Scenario to compare against the rules");
        VERBOSE("    Application:");
        mApplication.logItem();
        VERBOSE("    Device:");
        mDevice.logItem();
    }

  public:
    Application mApplication;
    Device mDevice;
};

// This encapsulates a Rule that provides an answer based on whether a particular Scenario matches
// the Rule.  A Rule always has an answer, but can potentially wildcard every item in it (i.e.
// match every scenario).
class Rule
{
  public:
    Rule(const std::string description, bool useANGLE)
        : mDescription(description),
          mAppList("Application"),
          mDevList("Device"),
          mUseANGLE(useANGLE)
    {}
    ~Rule() {}
    void addApp(const Application &app) { mAppList.addItem(app); }
    void addDevice(const Device &dev) { mDevList.addItem(dev); }
    bool match(const Scenario &toCheck) const
    {
        VERBOSE("    Within \"%s\" Rule: application match is %s and device match is %s\n",
                mDescription.c_str(), mAppList.match(toCheck.mApplication) ? "true" : "false",
                mDevList.match(toCheck.mDevice) ? "true" : "false");
        return (mAppList.match(toCheck.mApplication) && mDevList.match(toCheck.mDevice));
    }
    bool getUseANGLE() const { return mUseANGLE; }
    void logRule() const
    {
        VERBOSE("  Rule: \"%s\" %s ANGLE", mDescription.c_str(),
                mUseANGLE ? "enables" : "disables");
        mAppList.logListOf("    ", "Applications");
        mDevList.logListOf("    ", "Devices");
    }

    std::string mDescription;
    ListOf<Application> mAppList;
    ListOf<Device> mDevList;
    bool mUseANGLE;
};

// This encapsulates a list of Rules that Scenarios are matched against.  A Scenario is compared
// with each Rule, in order.  Any time a Scenario matches a Rule, the current answer is overridden
// with the answer of the matched Rule.
class RuleList
{
  public:
    RuleList() {}
    ~RuleList() { mRuleList.clear(); }

    static RuleList *ReadRulesFromJsonString(const std::string jsonFileContents)
    {
        RuleList *rules = new RuleList;

        // Open the file and start parsing it:
        Json::Reader jReader;
        Json::Value jTopLevelObject;
        jReader.parse(jsonFileContents, jTopLevelObject);
        Json::Value jRules = jTopLevelObject[kJsonRules];
        for (unsigned int ruleIndex = 0; ruleIndex < jRules.size(); ruleIndex++)
        {
            Json::Value jRule           = jRules[ruleIndex];
            std::string ruleDescription = jRule[kJsonRule].asString();
            bool useANGLE               = jRule[kJsonUseANGLE].asBool();
            Rule *newRule               = new Rule(ruleDescription, useANGLE);

            Json::Value jApps = jRule[kJsonApplications];
            for (unsigned int appIndex = 0; appIndex < jApps.size(); appIndex++)
            {
                Json::Value jApp    = jApps[appIndex];
                Application *newApp = Application::CreateApplicationFromJson(jApp);
                newRule->addApp(*newApp);
                delete newApp;
            }

            Json::Value jDevs = jRule[kJsonDevices];
            for (unsigned int deviceIndex = 0; deviceIndex < jDevs.size(); deviceIndex++)
            {
                Json::Value jDev = jDevs[deviceIndex];
                Device *newDev   = Device::CreateDeviceFromJson(jDev);

                Json::Value jGPUs = jDev[kJsonGPUs];
                for (unsigned int gpuIndex = 0; gpuIndex < jGPUs.size(); gpuIndex++)
                {
                    Json::Value jGPU = jGPUs[gpuIndex];
                    GPU *newGPU      = GPU::CreateGpuFromJson(jGPU);
                    if (newGPU)
                    {
                        newDev->addGPU(*newGPU);
                        delete newGPU;
                    }
                }
                newRule->addDevice(*newDev);
                delete newDev;
            }

            rules->addRule(*newRule);
            delete newRule;
        }

        // Make sure there is at least one, default rule.  If not, add it here:
        if (rules->mRuleList.size() == 0)
        {
            Rule defaultRule("Default Rule", false);
            rules->addRule(defaultRule);
        }
        return rules;
    }

    void addRule(const Rule &rule) { mRuleList.push_back(rule); }
    bool getUseANGLE(const Scenario &toCheck)
    {
        // Initialize useANGLE to the system-wide default (that should be set in the default
        // rule, but just in case, set it here too):
        bool useANGLE = false;
        VERBOSE("Checking scenario against %d ANGLE-for-Android rules:",
                static_cast<int>(mRuleList.size()));

        for (const Rule &rule : mRuleList)
        {
            VERBOSE("  Checking Rule: \"%s\" (to see whether there's a match)",
                    rule.mDescription.c_str());
            if (rule.match(toCheck))
            {
                VERBOSE("  -> Rule matches.  Setting useANGLE to %s",
                        rule.getUseANGLE() ? "true" : "false");
                useANGLE = rule.getUseANGLE();
            }
            else
            {
                VERBOSE("  -> Rule doesn't match.");
            }
        }

        return useANGLE;
    }
    void logRules()
    {
        VERBOSE("Showing %d ANGLE-for-Android rules:", static_cast<int>(mRuleList.size()));
        for (const Rule &rule : mRuleList)
        {
            rule.logRule();
        }
    }

  public:
    std::vector<Rule> mRuleList;
};

}  // namespace angle

extern "C" {

using namespace angle;

// This function is part of the version-2 API:
ANGLE_EXPORT bool ANGLEGetFeatureSupportUtilAPIVersion(unsigned int *versionToUse)
{
    if (!versionToUse || (*versionToUse < kFeatureVersion_LowestSupported))
    {
        // The versionToUse is either nullptr or is less than the lowest version supported, which
        // is an error.
        return false;
    }
    if (*versionToUse > kFeatureVersion_HighestSupported)
    {
        // The versionToUse is greater than the highest version supported; change it to the
        // highest version supported (caller will decide if it can use that version).
        *versionToUse = kFeatureVersion_HighestSupported;
    }
    return true;
}

// This function is part of the version-2 API:
ANGLE_EXPORT bool ANGLEAndroidParseRulesString(const char *rulesString,
                                               RulesHandle *rulesHandle,
                                               int *rulesVersion)
{
    if (!rulesString || !rulesHandle || !rulesVersion)
    {
        return false;
    }

    std::string rulesFileContents = rulesString;
    RuleList *rules               = RuleList::ReadRulesFromJsonString(rulesFileContents);
    rules->logRules();

    *rulesHandle  = rules;
    *rulesVersion = 0;
    return true;
}

// This function is part of the version-2 API:
ANGLE_EXPORT bool ANGLEGetSystemInfo(SystemInfoHandle *systemInfoHandle)
{
    if (!systemInfoHandle)
    {
        return false;
    }

    // TODO (http://anglebug.com/3036): Restore the real code
    angle::SystemInfo *systemInfo = new angle::SystemInfo;
    systemInfo->gpus.resize(1);
    GPUDeviceInfo &gpu = systemInfo->gpus[0];
    gpu.vendorId       = 0xFEFEFEFE;
    gpu.deviceId       = 0xFEEEFEEE;
    gpu.driverVendor   = "Foo";
    gpu.driverVersion  = "1.2.3.4";

    *systemInfoHandle = systemInfo;
    return true;
}

// This function is part of the version-2 API:
ANGLE_EXPORT bool ANGLEAddDeviceInfoToSystemInfo(const char *deviceMfr,
                                                 const char *deviceModel,
                                                 SystemInfoHandle systemInfoHandle)
{
    angle::SystemInfo *systemInfo = static_cast<angle::SystemInfo *>(systemInfoHandle);
    if (!deviceMfr || !deviceModel || !systemInfo)
    {
        return false;
    }

    systemInfo->machineManufacturer = deviceMfr;
    systemInfo->machineModelName    = deviceModel;
    return true;
}

// This function is part of the version-2 API:
ANGLE_EXPORT bool ANGLEShouldBeUsedForApplication(const RulesHandle rulesHandle,
                                                  int rulesVersion,
                                                  const SystemInfoHandle systemInfoHandle,
                                                  const char *appName)
{
    RuleList *rules               = static_cast<RuleList *>(rulesHandle);
    angle::SystemInfo *systemInfo = static_cast<angle::SystemInfo *>(systemInfoHandle);
    if (!rules || !systemInfo || !appName || (systemInfo->gpus.size() != 1))
    {
        return false;
    }

    Scenario scenario(appName, systemInfo->machineManufacturer.c_str(),
                      systemInfo->machineModelName.c_str());
    Version gpuDriverVersion(systemInfo->gpus[0].detailedDriverVersion.major,
                             systemInfo->gpus[0].detailedDriverVersion.minor,
                             systemInfo->gpus[0].detailedDriverVersion.subMinor,
                             systemInfo->gpus[0].detailedDriverVersion.patch);
    GPU gpuDriver(systemInfo->gpus[0].driverVendor, systemInfo->gpus[0].deviceId, gpuDriverVersion);
    scenario.mDevice.addGPU(gpuDriver);
    scenario.logScenario();

    bool rtn = rules->getUseANGLE(scenario);
    VERBOSE("Application \"%s\" should %s ANGLE", appName, rtn ? "use" : "NOT use");

    return rtn;
}

// This function is part of the version-2 API:
ANGLE_EXPORT void ANGLEFreeRulesHandle(const RulesHandle rulesHandle)
{
    RuleList *rules = static_cast<RuleList *>(rulesHandle);
    if (rules)
    {
        delete rules;
    }
}

// This function is part of the version-2 API:
ANGLE_EXPORT void ANGLEFreeSystemInfoHandle(const SystemInfoHandle systemInfoHandle)
{
    angle::SystemInfo *systemInfo = static_cast<angle::SystemInfo *>(systemInfoHandle);
    if (systemInfo)
    {
        delete systemInfo;
    }
}

}  // extern "C"
