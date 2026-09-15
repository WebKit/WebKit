#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AudioComponent.h>)
/*!
	@file		AudioComponent.h
 	@framework	AudioToolbox.framework
 	@copyright	(c) 2007-2020 Apple Inc. All rights reserved.
	@brief		API's to locate, get information about, and open audio components.

	@discussion

	This header file defines a collection of APIs to find, get information about, and open
	audio components (such as audio units, audio codecs, and audio file components).

	Originally, CoreServices' Component Manager was used for the registration, discovery, and
	packaging of these loadable code modules. However, in order to provide an API that will be
	supported going forward from macOS 10.6 and iOS 2.0, it is advised that applications use the
	Audio Component APIs to find and load (open) audio components such as audio units.

	The type "AudioComponent" or "AudioComponentInstance" should be seen and used as a distinct type
	from the Component Manager types of "Component" and "ComponentInstance". It is never safe to
	assume a direct cast is compatible between this type and the other.

	Beginning with macOS 10.7, AudioComponents can be registered and used directly without
	involving the Component Manager. The system scans certain directories for bundles with names
	ending in ".audiocomp" or ".component" (the latter permits registering plug-ins in a single
	bundle with both the Component Manager and the Audio Component system). These directories are
	scanned non-recursively:

	- ~/Library/Audio/Plug-Ins/Components
	- /Library/Audio/Plug-Ins/Components
	- /System/Library/Components

	Bundles' Info.plist dictionaries should contain an "AudioComponents" item whose value
	is an array of dictionaries, e.g.

	```
		<key>AudioComponents</key>
		<array>
			<dict>
				<key>type</key>
				<string>aufx</string>
				<key>subtype</key>
				<string>XMPL</string>
				<key>manufacturer</key>
				<string>ACME</string>
				<key>name</key>
				<string>AUExample</string>
				<key>version</key>
				<integer>12345</integer>
				<key>factoryFunction</key>
				<string>AUExampleFactory</string>
				
				<!-- An AudioComponent is sandbox safe -->
				
				<key>sandboxSafe</key>
				<true/>
				
				<!-- or it can describe its resource usage -->
				
				<key>resourceUsage</key>
				<dict>
					<key>iokit.user-client</key>
					<array>
						<string>CustomUserClient1</string>
						<string>CustomUserClient2</string>
					</array>
					<key>mach-lookup.global-name</key>
					<array>
						<string>MachServiceName1</string>
						<string>MachServiceName2</string>
					</array>
					<key>network.client</key>
					<true/>
					<key>temporary-exception.files.all.read-write</key>
					</true>
				</dict>

				<!-- An AudioComponent can define its tags -->
				
				<key>tags</key>
				<array>
					<string>Effect</string>
					<string>Equalizer</string>
				</array>
			</dict>
		</array>
	```

	The type, subtype and manufacturer keys correspond to the OSType fields of the
	AudioComponentDescription structure. They can be strings if they are 4 ASCII characters;
	otherwise they must be 32-bit integers.

	The "factoryFunction" is the name of a AudioComponentFactoryFunction in the bundle's binary.


	Sandbox-Safety
	--------------

	The "sandboxSafe" key is used to indicate whether or not an AudioComponent can be loaded
	directly into a sandboxed process. This key is reflected in the componentFlags field of the the
	AudioComponentDescription for the AudioComponent with the constant,
	kAudioComponentFlag_SandboxSafe. Note that if this key is not present, it is assumed that the
	AudioComponent is not sandbox safe.

	The "resourceUsage" key describes the system resources used by an AudioComponent that is not
	sandbox safe. The keys for this dictionary are described below. If the "sandboxSafe" key is
	true, this dictionary should not be included.

	The "iokit.user-client" key is a "resourceUsage" key that describes the IOKit user-client
	objects the AudioComponent will open. It is an array of the user-clients' class names.

	The "mach-lookup.global-name" key is a "resourceUsage" key that describes the mach services the
	AudioComponent needs to connect to. It is an array of the names of the services. Note that these
	services can be direct mach services found via bootstrap_look_up() or XPC services found via
	xpc_connection_create_mach_service().

	The "network.client" key is a "resourceUsage" key that indicates that the AudioComponent will
	receive data from the network.

	The "temporary-exception.files.all.read-write" key is a "resourceUsage" key that indicates that
	the AudioComponent needs arbitrary access to the file system. This is for backward compatibility
	for AudioComponents that have not yet adopted the usage of security scope bookmarks and/or the
	usage of the standard file dialog for discovering, accessing and storing persistent references
	to files on the file system. In a future OS release, this key will not be supported.

	Note that a sandbox-safe AudioComponent can function correctly in even the most severely
	sandboxed process. This means that the process will have curtailed or no access to common system
	resources like the file system, device drivers, the network, and communication with other
	processes.

	When instantiating a sandbox unsafe AudioComponent in a sandboxed process, the system evaluates
	the "resourceUsage" information against the restrictions the process is under. If the
	"resourceUsage" will not violate those restrictions, the AudioComponent will be instantiated and
	can be used as normal. Note that the system will set kAudioComponentFlag_SandboxSafe in the
	AudioComponentDescription in this case.

	If the "resourceUsage" information includes things that can't be accessed from the process and
	the process has the entitlement, "com.apple.security.temporary-exception.audio-unit-host", the
	system will ask the user whether or not it is acceptable for the process to open the unsafe
	AudioComponent. If the user says yes, the system will suspend the process's sandbox and allow
	the unsafe AudioComponent to be opened and used.


	Tags
	----

	The "tags" key is an array of tags associated with the defined AudioComponent. The following are
	the set of predefined standard tags that are localized and can be used in the audio unit
	definition. "Equalizer", "Dynamics", "Distortion", "Synthesizer", "Effects", "Filter", "Dynamics
	Processor", "Delay", "Reverb", "Pitch", "Panner", "Imaging", "Sampler", "Mixer", "Format
	Converter", "Time Effect", "Output", "Offline Effect", "Drums", "Guitar", "Vocal", "Bass",
	"MIDI". 

	These standard tags should not be localized in the audio unit.

	Localizing the tags is similar to localizing AudioUnit parameter strings. Create a strings 
	resource file and name it "AudioUnitTags.strings".
	For more information on strings resource file please check
	https://developer.apple.com/library/mac/documentation/macosx/conceptual/bpinternational/Articles/StringsFiles.html
*/

#ifndef AudioUnit_AudioComponent_h
#define AudioUnit_AudioComponent_h

//=====================================================================================================================
#pragma mark Overview

#include <TargetConditionals.h>
#include <Availability.h>
#include <CoreAudioTypes/CoreAudioTypes.h>
#include <CoreFoundation/CoreFoundation.h>

CF_ASSUME_NONNULL_BEGIN

//=====================================================================================================================
#pragma mark Constants

/*!
	@enum		AudioComponentFlags
	@brief		Flags found in AudioComponentDescription.componentFlags.
	
	@constant	kAudioComponentFlag_Unsearchable

	When this bit in AudioComponentDescription's componentFlags is set, AudioComponentFindNext
	will only return this component when performing a specific, non-wildcard search for the
	component, i.e. with non-zero values of componentType, componentSubType, and
	componentManufacturer. This can be useful when privately registering a component.
	
	@constant	kAudioComponentFlag_SandboxSafe
	
	An AudioComponent sets this bit in its componentFlags to indicate to the system that the
	AudioComponent is safe to open in a sandboxed process.
	
	@constant	kAudioComponentFlag_IsV3AudioUnit
	
	The system sets this flag automatically when registering components which implement a version 3
	Audio Unit.
	
	@constant	kAudioComponentFlag_RequiresAsyncInstantiation
	
	The system sets this flag automatically when registering components which require asynchronous
	instantiation via AudioComponentInstantiate (v3 audio units with views).
	
	@constant	kAudioComponentFlag_CanLoadInProcess
	
	The system sets this flag automatically when registering components which can be loaded into
	the current process. This is always true for V2 audio units; it depends on the packaging
	in the case of a V3 audio unit.
*/
typedef CF_OPTIONS(UInt32, AudioComponentFlags) {
    kAudioComponentFlag_Unsearchable                CF_ENUM_AVAILABLE(10_7, 5_0)   = 1,
    kAudioComponentFlag_SandboxSafe                 CF_ENUM_AVAILABLE(10_8, 6_0)   = 2,
    kAudioComponentFlag_IsV3AudioUnit               CF_ENUM_AVAILABLE(10_11, 9_0)  = 4,
    kAudioComponentFlag_RequiresAsyncInstantiation  CF_ENUM_AVAILABLE(10_11, 9_0)  = 8,
	kAudioComponentFlag_CanLoadInProcess			CF_ENUM_AVAILABLE(10_11, 9_0)  = 0x10
};

/*! @enum       AudioComponentInstantiationOptions
    @brief      Options controlling component instantiation.
    @discussion
        Most component instances are loaded into the calling process.

        A version 3 audio unit, however, can be loaded into a separate extension service process,
        and this is the default behavior for these components. To be able to load one in-process
        requires that the developer package the audio unit in a bundle separate from the application
        extension, since an extension's main binary cannot be dynamically loaded into another
        process.
        
        A macOS host may request in-process loading of such audio units using
        kAudioComponentInstantiation_LoadInProcess.

        kAudioComponentFlag_IsV3AudioUnit specifies whether an audio unit is implemented using API
        version 3.

        These options are just requests to the implementation. It may fail and fall back to the
        default.
    @constant kAudioComponentInstantiation_LoadOutOfProcess
        Attempt to load the component into a separate extension process.
    @constant kAudioComponentInstantiation_LoadInProcess
        Attempt to load the component into the current process. Only available on macOS.
*/
typedef CF_OPTIONS(UInt32, AudioComponentInstantiationOptions) {
    kAudioComponentInstantiation_LoadOutOfProcess   CF_ENUM_AVAILABLE(10_11,  9_0) = 1,
    kAudioComponentInstantiation_LoadInProcess      API_AVAILABLE(macos(10.11)) API_UNAVAILABLE(ios)  = 2,
    kAudioComponentInstantiation_LoadedRemotely     API_UNAVAILABLE(macos)    = 1u << 31,
};


//=====================================================================================================================
#pragma mark Data Types

/*!
    @struct         AudioComponentDescription
    @discussion     A structure describing the unique and identifying IDs of an audio component
    @var            componentType
                        A 4-char code identifying the generic type of an audio component.
    @var            componentSubType
                        A 4-char code identifying the a specific individual component. type/
                        subtype/manufacturer triples must be globally unique.
    @var            componentManufacturer
                        Vendor identification.
    @var            componentFlags
                        Must be set to zero unless a known specific value is requested.
    @var            componentFlagsMask
                        Must be set to zero unless a known specific value is requested.
*/
#pragma pack(push, 4)
typedef struct AudioComponentDescription {
    OSType              componentType;
    OSType              componentSubType;
    OSType              componentManufacturer;
    UInt32              componentFlags;
    UInt32              componentFlagsMask;
} AudioComponentDescription;
#pragma pack(pop)

/*!
    @typedef        AudioComponent
    @abstract       The type used to represent a class of particular audio components
    @discussion     An audio component is usually found through a search and is then uniquely
                    identified by the triple of an audio component's type, subtype and
                    manufacturer.
                    
                    It can have properties associated with it (such as a name, a version).

                    It is then used as a factory (like a class in an object-oriented programming
                    language) from which to create instances. The instances are used to do the
                    actual work.

                    For example: the AudioComponentDescription 'aufx'/'dely'/'appl' describes the
                    delay audio unit effect from Apple, Inc. You can find this component by
                    searching explicitly for the audio component that matches this pattern (this is
                    an unique identifier - there is only one match to this triple ID). Then once
                    found, instances of the Apple delay effect audio unit can be created from its
                    audio component and used to apply that effect to an audio signal. A single
                    component can create any number of component instances.
*/
typedef struct OpaqueAudioComponent *   AudioComponent;

/*!
    @typedef        AudioComponentInstance
    @abstract       The type used to represent an instance of a particular audio component
    @discussion     An audio component instance is created from its factory/producer audio
                    component. It is the body of code that does the work.
    
                    A special note: While on the desktop this is typedef'd to a
                    ComponentInstanceRecord *, you should not assume that this will always be
                    compatible and usable with Component Manager calls.
*/
#if TARGET_OS_IPHONE || (0 && 0) || TARGET_OS_LINUX || (defined(AUDIOCOMPONENT_NOCARBONINSTANCES) && AUDIOCOMPONENT_NOCARBONINSTANCES)
    typedef struct OpaqueAudioComponentInstance *   AudioComponentInstance;
#else
    typedef struct ComponentInstanceRecord *        AudioComponentInstance;
#endif

/*!
    @typedef        AudioComponentMethod
    @abstract       Generic prototype for an audio plugin method.
    @discussion     Every audio plugin will implement a collection of methods that match a particular
					selector. For example, the AudioUnitInitialize API call is implemented by a
					plugin implementing the kAudioUnitInitializeSelect selector. Any function implementing
					an audio plugin selector conforms to the basic pattern where the first argument
					is a pointer to the plugin instance structure, has 0 or more specific arguments,  
					and returns an OSStatus.
*/
typedef OSStatus (*AudioComponentMethod)(void *self, ...);

/*!
    @struct         AudioComponentPlugInInterface
    @discussion     A structure used to represent an audio plugin's routines 
    @var            Open
                        the function used to open (or create) an audio plugin instance
    @var            Close
                        the function used to close (or dispose) an audio plugin instance
    @var            Lookup
                        this is used to return a function pointer for a given selector, 
						or NULL if that selector is not implemented
    @var            reserved
                        must be NULL
*/
struct AudioComponentPlugInInterface {
	OSStatus						(*Open)(void *self, AudioComponentInstance mInstance);
	OSStatus						(*Close)(void *self);
	AudioComponentMethod __nullable	(* __nonnull Lookup)(SInt16 selector);
	void * __nullable				reserved;
};
typedef struct AudioComponentPlugInInterface AudioComponentPlugInInterface;

/*!
    @typedef        AudioComponentFactoryFunction
    @abstract       A function that creates AudioComponentInstances.
    @discussion
                    Authors of AudioComponents may register them from bundles as described
                    above in the discussion of this header file, or dynamically within a single
                    process, using AudioComponentRegister.
    
    @param          inDesc
                        The AudioComponentDescription specifying the component to be instantiated.
    @result         A pointer to a AudioComponentPlugInInterface structure.
*/
typedef AudioComponentPlugInInterface * __nullable (*AudioComponentFactoryFunction)(const AudioComponentDescription *inDesc);

//=====================================================================================================================
#pragma mark Functions

#ifdef __cplusplus
extern "C" {
#endif

/*!
    @function       AudioComponentFindNext
    @abstract       Finds an audio component.
    @discussion     This function is used to find an audio component that is the closest match
                    to the provided values. Note that the list of available components may change
					dynamically in situations involving inter-app audio on iOS, or version 3
					audio unit extensions. See kAudioComponentRegistrationsChangedNotification.

    @param          inComponent
                        If NULL, then the search starts from the beginning until an audio
                        component is found that matches the description provided by inDesc.
                        If non-NULL, then the search starts (continues) from the previously
                        found audio component specified by inComponent, and will return the next
                        found audio component.
    @param          inDesc
                        The type, subtype and manufacturer fields are used to specify the audio
                        component to search for. A value of 0 (zero) for any of these fields is
                        a wildcard, so the first match found is returned.
    @result         An audio component that matches the search parameters, or NULL if none found.
*/
extern AudioComponent __nullable
AudioComponentFindNext (    AudioComponent __nullable           inComponent,
                            const AudioComponentDescription *   inDesc) 
                                                                            API_AVAILABLE(macos(10.6), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function       AudioComponentCount
    @abstract       Counts audio components.
    @discussion     Returns the number of AudioComponents that match the specified
                    AudioComponentDescription.
    @param          inDesc
                        The type, subtype and manufacturer fields are used to specify the audio
                        components to count A value of 0 (zero) for any of these fields is a
                        wildcard, so will match any value for this field
    @result         a UInt32. 0 (zero) means no audio components were found that matched the
                    search parameters.
*/
extern UInt32
AudioComponentCount (   const AudioComponentDescription *       inDesc)
                                                                            API_AVAILABLE(macos(10.6), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function       AudioComponentCopyName
    @abstract       Retrieves the name of an audio component.
    @discussion     the name of an audio component
    @param          inComponent
                        the audio component (must not be NULL)
    @param          outName
                        a CFString that is the name of the audio component. This string should
                        be released by the caller.
    @result         an OSStatus result code.
*/
extern OSStatus 
AudioComponentCopyName (    AudioComponent                      inComponent, 
                            CFStringRef __nullable * __nonnull  outName)
                                                                            API_AVAILABLE(macos(10.6), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function       AudioComponentGetDescription
    @abstract       Retrieve an audio component's description.
    @discussion     This will return the fully specified audio component description for the
                    provided audio component.
    @param          inComponent
                        the audio component (must not be NULL)
    @param          outDesc
                        the audio component description for the specified audio component
    @result         an OSStatus result code.
*/
extern OSStatus 
AudioComponentGetDescription(   AudioComponent                  inComponent,
                                AudioComponentDescription *     outDesc)
                                                                            API_AVAILABLE(macos(10.6), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function       AudioComponentGetVersion
    @abstract       Retrieve an audio component's version.
    @param          inComponent
                        the audio component (must not be NULL)
    @param          outVersion
                        the audio component's version in the form of 0xMMMMmmDD (Major, Minor, Dot)
    @result         an OSStatus result code.
*/
extern OSStatus 
AudioComponentGetVersion(   AudioComponent                      inComponent, 
                            UInt32 *                            outVersion)
                                                                            API_AVAILABLE(macos(10.6), ios(2.0), watchos(2.0), tvos(9.0));

#if defined(__OBJC__) && !TARGET_OS_IPHONE && !(0 && 0)
@class NSImage;

/*!
    @function       AudioComponentGetIcon
    @abstract       Fetches an icon representing the component.
    @param          comp
        The component whose icon is to be retrieved.
    @result
        An autoreleased NSImage object.
    @discussion
        For a component originating in an app extension, the returned icon will be that of the
        application containing the extension.
        
        For components loaded from bundles, the icon will be that of the bundle.
*/
extern NSImage * __nullable
AudioComponentGetIcon(AudioComponent comp) API_DEPRECATED_WITH_REPLACEMENT("AudioComponentCopyIcon", macos(10.11, 11.0)) API_UNAVAILABLE(ios, watchos, tvos);
#endif

/*!
    @function       AudioComponentInstanceNew
    @abstract       Creates an audio component instance.
    @discussion     This function creates an instance of a given audio component. The audio
                    component instance is the object that does all of the work, whereas the
                    audio component is the way an application finds and then creates this object
                    to do this work. For example, an audio unit is a type of audio component
                    instance, so to use an audio unit, one finds its audio component, and then
                    creates a new instance of that component. This instance is then used to
                    perform the audio tasks for which it was designed (process, mix, synthesise,
                    etc.).
    @param          inComponent
                        the audio component (must not be NULL)
    @param          outInstance
                        the audio component instance
    @result         an OSStatus result code.
*/
extern OSStatus 
AudioComponentInstanceNew(      AudioComponent                                inComponent,
                                AudioComponentInstance __nullable * __nonnull outInstance)
                                                                            API_AVAILABLE(macos(10.6), ios(2.0), watchos(2.0), tvos(9.0));
/*!
    @function       AudioComponentInstantiate
    @abstract       Creates an audio component instance, asynchronously.
    @discussion     This is an asynchronous version of AudioComponentInstanceNew(). It must be
                    used to instantiate any component with kAudioComponentFlag_RequiresAsyncInstantiation
                    set in its component flags. It may be used for other components as well.
					
					Note: Do not block the main thread while waiting for the completion handler
					to be called; this can deadlock.
    @param          inComponent
                        the audio component
    @param          inOptions
                        see AudioComponentInstantiationOptions
    @param          inCompletionHandler
                        called in an arbitrary thread context when instantiation is complete.
*/
extern void
AudioComponentInstantiate(      AudioComponent                      inComponent,
                                AudioComponentInstantiationOptions  inOptions,
                                void (^inCompletionHandler)(AudioComponentInstance __nullable, OSStatus))
                                                                            API_AVAILABLE(macos(10.11), ios(9.0), watchos(2.0), tvos(9.0));

/*!
    @function       AudioComponentInstanceDispose
    @abstract       Disposes of an audio component instance.
    @discussion     This function will dispose the audio component instance that was created
                    with the New call. It will deallocate any resources that the instance was using.
    @param          inInstance
                        the audio component instance to dispose (must not be NULL)
    @result         an OSStatus result code.
*/
extern OSStatus 
AudioComponentInstanceDispose(  AudioComponentInstance          inInstance)
                                                                            API_AVAILABLE(macos(10.6), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function       AudioComponentInstanceGetComponent
    @abstract       Retrieve the audio component from its instance
    @discussion     Allows the application at any time to retrieve the audio component that is
                    the factory object of a given instance (i.e., the audio component that was
                    used to create the instance in the first place). This allows the application
                    to retrieve general information about a particular audio component (its
                    name, version, etc) when one just has an audio component instance to work
                    with
    @param          inInstance
                        the audio component instance (must not be NULL, and instance must be valid - that is, not disposed)
    @result         a valid audio component or NULL if no component was found.
*/
extern AudioComponent 
AudioComponentInstanceGetComponent (    AudioComponentInstance      inInstance)
                                                                            API_AVAILABLE(macos(10.6), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function       AudioComponentInstanceCanDo
    @discussion     Determines if an audio component instance implements a particular component
                    API call as signified by the specified selector identifier token.
    @param          inInstance
                        the audio component instance
    @param          inSelectorID
                        a number to signify the audio component API (component selector) as appropriate for the instance's component type.
    @result         a boolean
*/
extern Boolean 
AudioComponentInstanceCanDo (   AudioComponentInstance              inInstance, 
                                SInt16                              inSelectorID)
                                                                            API_AVAILABLE(macos(10.6), ios(3.0), watchos(2.0), tvos(9.0));

/*!
    @function       AudioComponentRegister
    @abstract       Dynamically registers an AudioComponent within the current process
    @discussion
        AudioComponents are registered either when found in appropriate bundles in the filesystem,
        or via this call. AudioComponents registered via this call are available only within
        the current process.
    
    @param          inDesc
                        The AudioComponentDescription that describes the AudioComponent. Note that
                        the registrar needs to be sure to set the flag kAudioComponentFlag_SandboxSafe
                        in the componentFlags field of the AudioComponentDescription to indicate that
                        the AudioComponent can be loaded directly into a sandboxed process.
    @param          inName
                        the AudioComponent's name
    @param          inVersion
                        the AudioComponent's version
    @param          inFactory
                        an AudioComponentFactoryFunction which will create instances of your
                        AudioComponent
    @result         an AudioComponent object
*/
extern AudioComponent
AudioComponentRegister(     const AudioComponentDescription *   inDesc,
                            CFStringRef                         inName,
                            UInt32                              inVersion,
                            AudioComponentFactoryFunction       inFactory)
                                                    API_AVAILABLE(macos(10.7), ios(5.0), watchos(2.0), tvos(9.0));

/*!
    @function       AudioComponentCopyConfigurationInfo
    @abstract       Fetches the basic configuration info about a given AudioComponent
    @discussion     Currently, only AudioUnits can supply this information.
    @param          inComponent
                        The AudioComponent whose info is being fetched.
    @param          outConfigurationInfo
                        On exit, this is CFDictionaryRef that contains information describing the
                        capabilities of the AudioComponent. The specific information depends on the
                        type of AudioComponent. The keys for the dictionary are defined in
                        AudioUnitProperties.h (or other headers as appropriate for the component type).
    @result         An OSStatus indicating success or failure.
*/
extern OSStatus
AudioComponentCopyConfigurationInfo(    AudioComponent      inComponent,
                                        CFDictionaryRef __nullable * __nonnull outConfigurationInfo)
                                                    API_AVAILABLE(macos(10.7), ios(16.0)) API_UNAVAILABLE(watchos, tvos);

/*!
	 @enum		 AudioComponentValidationResult
	 @abstract	 Constants for describing the result of validating an AudioComponent
	 @constant	 kAudioComponentValidationResult_Passed
					The AudioComponent passed validation.
	 @constant	 kAudioComponentValidationResult_Failed
					The AudioComponent failed validation.
	 @constant	 kAudioComponentValidationResult_TimedOut
					The validation operation timed out before completing.
	 @constant	 kAudioComponentValidationResult_UnauthorizedError_Open
					The AudioComponent failed validation during open operation as it is not authorized.
	 @constant	 kAudioComponentValidationResult_UnauthorizedError_Init
					The AudioComponent failed validation during initialization as it is not authorized.
*/
typedef CF_ENUM(UInt32, AudioComponentValidationResult)
{
	kAudioComponentValidationResult_Unknown = 0,
	kAudioComponentValidationResult_Passed,
	kAudioComponentValidationResult_Failed,
	kAudioComponentValidationResult_TimedOut,
	kAudioComponentValidationResult_UnauthorizedError_Open,
	kAudioComponentValidationResult_UnauthorizedError_Init
};
	
/*!
	@define		kAudioComponentConfigurationInfo_ValidationResult
	@abstract	Dictionary that contains the AudioComponentValidationResult for the component.
	@discussion
		The keys in this dictionary are the CPU architectures (e.g. "i386") that generated each result.
*/
#define kAudioComponentConfigurationInfo_ValidationResult	"ValidationResult"
	
/*!
	@function		AudioComponentValidate
	@abstract		Tests a specified AudioComponent for API and behavioral conformance.
	@discussion	Currently, only AudioUnits can can be validated.
	@param			inComponent
						The AudioComponent to validate.
	@param			inValidationParameters
						A CFDictionaryRef that contains parameters for the validation operation.
						Passing NULL for this argument tells the system to use the default
						parameters.
	@param			outValidationResult
						On exit, this is an AudioComponentValidationResult.
	@result			an OSStatus result code.
*/
#if !(0 && 0)
extern OSStatus
AudioComponentValidate( AudioComponent					inComponent,
						CFDictionaryRef __nullable		inValidationParameters,
						AudioComponentValidationResult *outValidationResult)
                                                    API_AVAILABLE(macos(10.7), ios(16.0)) API_UNAVAILABLE(watchos, tvos);

/*!
	@function		AudioComponentValidateWithResults
	@abstract		Tests a specified AudioComponent for API and behavioral conformance
					asynchronously, returning detailed validation results.
	@discussion		Currently, only AudioUnits can can be validated. The `inCompletionHandler` callback
					has two parameters, an `AudioComponentValidationResult` with result of the validation,
					and a `CFDictionaryRef` which contains the details of this result.
					This dictionary may contain the following entries:
						"Output"
							An array of strings, with the same content as if the AU was validated on auval.
						"Result"
							An `AudioComponentValidationResult` with the result of the validation
							process. The same as what's in the `AudioComponentValidationResult`
							in the `inCompletionHandler` and what `AudioComponentValidate`
							currently returns.
						"Tests"
							An array in which each value is a dictionary and may contain:
								"Name"
									A descriptive name of the test.
								"Result"
									An `AudioComponentValidationResult` with the result of the
									specific test.
								"Output"
									An array of strings with output generated by the test.
						"WasCached"
							`YES` if the returned result was cached from previous runs.
	@param			inComponent
						The AudioComponent to validate.
	@param			inValidationParameters
						A CFDictionaryRef that contains parameters for the validation operation.
						Passing NULL for this argument tells the system to use the default
						parameters.
	@param			inCompletionHandler
						Completion callback. See discussion section.
	@result			an OSStatus result code.
*/
extern OSStatus
AudioComponentValidateWithResults(AudioComponent				inComponent,
								  CFDictionaryRef __nullable	inValidationParameters,
								  void 							(^inCompletionHandler)(AudioComponentValidationResult, CFDictionaryRef))
													API_AVAILABLE(macos(13.0), ios(16.0)) API_UNAVAILABLE(watchos, tvos);
#endif
/*!
	@define		kAudioComponentValidationParameter_TimeOut
	@discussion This is a number that indicates the time in seconds to wait for a validation
				operation to complete. Note that if a validation operation times out, it will return
				kAudioComponentValidationResult_TimedOut as its result.
*/
#define kAudioComponentValidationParameter_TimeOut				"TimeOut"
	
/*!
	 @define	 kAudioComponentValidationParameter_ForceValidation
	 @discussion
	 	This is a bool that indicates to ignore the cached value and run validation on the specified
	 	audio unit and update the cache.
*/
#define kAudioComponentValidationParameter_ForceValidation		 "ForceValidation"

/*!
	 @define	 kAudioComponentValidationParameter_LoadOutOfProcess
	 @discussion
		This is a bool that can be used when validating Audio Units and it specifies that the
		Audio Unit should be loaded out-of-process during validation.
		Under normal circumstances, the validation result should not be influenced by how
		the Audio Unit is loaded (in- or out-of-process). This option allows a host
		that plans on loading the Audio Unit out-of-process to make sure that it passes the
		validation checks in this mode of operation.
*/
#define kAudioComponentValidationParameter_LoadOutOfProcess		 "LoadOutOfProcess"

#ifdef __cplusplus
}
#endif

CF_ASSUME_NONNULL_END

#endif // AudioUnit_AudioComponent_h
#else
#include <AudioToolboxCore/AudioComponent.h>
#endif
