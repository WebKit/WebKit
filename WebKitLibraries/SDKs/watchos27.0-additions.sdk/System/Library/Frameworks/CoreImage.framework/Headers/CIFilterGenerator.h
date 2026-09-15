/* CoreImage - CIFilterGenerator.h

 Copyright (c) 2015 Apple, Inc.
   All rights reserved. */

#ifndef CIFILTERGENERATOR_H
#define CIFILTERGENERATOR_H

#ifdef __OBJC__

#import <CoreImage/CoreImageDefines.h>
#import <CoreImage/CIFilterConstructor.h>


@class NSURL;
@class NSDictionary;
@class CIFilter;

NS_ASSUME_NONNULL_BEGIN


/** The key of the target object that is exported */
CORE_IMAGE_EXPORT NSString *const kCIFilterGeneratorExportedKey NS_AVAILABLE_MAC(10_5);

/** Target object for the exported key */
CORE_IMAGE_EXPORT NSString *const kCIFilterGeneratorExportedKeyTargetObject NS_AVAILABLE_MAC(10_5);

/** Name of the key under which it is exported. */
CORE_IMAGE_EXPORT NSString *const kCIFilterGeneratorExportedKeyName NS_AVAILABLE_MAC(10_5);



/** The goal is to CIFilters to be connected and form a single CIFilter for ease of reusability.
 
 The CIFilterGenerator allows developers to create complex effects built out of one or more CIFilter and reuse them without changing code. The resulting CIFilterGenerator can be written into a file for which we introduce a new file type (extension). A CIFilterGenerator can be created from the API or more conveniently through an editor view that we provide.
 CIFilterGenerator files can be put into the Image Units folders on the system and they will be loaded when the user invokes one of the loadPlugIns methods. They will be registered by their filename or if present by an attribute in its description.
 */
NS_CLASS_AVAILABLE_MAC(10_5)
@interface CIFilterGenerator : NSObject <NSSecureCoding, NSCopying, CIFilterConstructor>
{
    @private struct CIFilterGeneratorStruct *_filterGeneratorStruct;
}

/** This creates an empty CIFilterGenerator in which you connect filters and images. */
+ (CIFilterGenerator *)filterGenerator;


/** Create a CIFilterGenerator with the contents of the file.
 @result   CIFilterGenerator object. If the file could not be read it returns nil.
*/
+ (nullable CIFilterGenerator *)filterGeneratorWithContentsOfURL:(NSURL *)aURL;


/** Initializes a CIFilterGenerator with the contents of the file.

 @result   CIFilterGenerator object. If the file could not be read it returns nil.
*/
- (nullable id)initWithContentsOfURL:(NSURL *)aURL;


/** Connect two objects into the filter chain.

 This method connects two object in the filter chain. For instance you can connect the outputImage key of a CISepiaTone filter object to the inputImage key of another CIFilter.
 @param   sourceObject  A CIFilter, CIImage, NSString, or NSURL describing the path to the image
 @param   sourceKey     For KVC access to the source object. Can be nil which means that the source object will be used directly.
 @param   targetObject  The object that you link the source object to.
 @param   targetKey     The key that you assign the source object to.
*/
- (void)connectObject:(id)sourceObject
			  withKey:(nullable NSString *)sourceKey
			 toObject:(id)targetObject
			  withKey:(NSString *)targetKey;


/** Removes the connection between two objects in the filter chain.
    
 Use this method to disconnect two objects that you connected using the connectObject:withKey:toObject:withKey: method.
 @param      sourceObject A CIFilter or CIImage or an NSString or an NSURL describing the path to the image
 @param      sourceKey For KVC access to the source object. Can be nil which means that the source object will be used directly.
 @param      targetObject The object that you linked the source object to.
 @param      targetKey The key that you assigned the source object to.
*/
- (void)disconnectObject:(id)sourceObject
                 withKey:(NSString *)sourceKey
                toObject:(id)targetObject
                 withKey:(NSString *)targetKey;


/** This methods allows you to export an input or output key of an object in the filter chain to be available through the inputKeys or outputKeys API when converted into a CIFilter
 
 When you create a CIFilter from the CIFilterGenerator, you might want the client of the filter being able to set some of the parameters of the filter chain. To do so these parameters have to be exported as keys much like the inputKeys and outputKeys of all CIFilters.
 @param      key The key path that is to be exported from the target object (eg. inputImage)
 @param      targetObject The object of which the key is to be exported (eg the filter).
 @param      exportedKeyName The name under which you want the new key to be available. This parameter can be nil in which case the original key name will be used. This name has to be unique. If a key being exported is an inputKey of the filter it will be exported as an input key and the other way around for output keys.
*/
- (void)exportKey:(NSString *)key
       fromObject:(id)targetObject
         withName:(nullable NSString *)exportedKeyName;


/** Removes a key that was exported before using exportKey:fromObject:withName:
 
 Use this method when you want to remove a prior exported key. It will not show up under inputKeys or outputKeys anymore.
 @param      exportedKeyName Name of the key that was exported.
*/
- (void)removeExportedKey:(NSString *)exportedKeyName;


/** An array of the exported keys.
 
 Use this method to get an NSArray of all the keys that you have exported using exportKey:fromObject:withName: or that were exported before written to a file from which you read the filter chain.
 @result     An array of dictionaries that describe the exported key and target object. See CIExportedKey, CIExportedKeyTargetObject and CIExportedKeyName for keys used in the dictionary.
*/
@property (readonly, nonatomic) NSDictionary *exportedKeys;


/** Set a new dictionary of attributes for an exported key.
    
 By default, the exported key inherits the attributes from its original key and target object. Use this method to for instance change the default value or lower the maximum allowed value. 
 */
- (void)setAttributes:(NSDictionary *)attributes
       forExportedKey:(NSString *)key;


/** Retrieve or Set the class attributes that will be used to register the filter using the registerFilterName method.
 Make sure you set the class attributes before using the registerFilterName method.
 See CIFilter for a description of the classAttributes that are needed to register a filter. 
*/
@property (retain, nonatomic) NSDictionary * classAttributes;


/** Create a CIFilter object based on this filter chain.
 
 This method creates a CIFilter from the filter chain where the topology of the chain is immutable, meaning that changes to the filter chain will not be reflected in the filter. The filter will have the input and output keys that were exported as described above.
*/
- (CIFilter *)filter;


/** Register the resulting filter of the chain in the CIFilter repository.
 
 This method allows you to register the filter chain as a named filter in the filter repository. You can then create a CIFilter object from it using the filterWithName: method. Make sure you set the class attributes first - see CIFilter for a description of the classAttributes that are needed to register a filter.
 When registering Core Image automatically adds the kCIFilterGeneratorCategory to the filters categories. The kCIFilterGeneratorCategory is purely for identification purpose and will not be exposed in the filter browser as a separate category.
 
 @param      name The name under which the filter will be registered. This name has to be unique. 
*/
- (void)registerFilterName:(NSString *)name;


/** Write the CIFilterGenerator into a file
 @result     Returns true when the chain was written out successfully 
*/
- (BOOL)writeToURL:(NSURL *)aURL atomically:(BOOL)flag;

@end

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CIFILTERGENERATOR_H */
