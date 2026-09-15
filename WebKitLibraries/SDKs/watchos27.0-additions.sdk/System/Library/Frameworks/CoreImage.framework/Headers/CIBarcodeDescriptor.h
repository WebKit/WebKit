/* CoreImage - CIBarcodeDescriptor.h

   Copyright (c) 2017 Apple, Inc.
   All rights reserved. */

#ifndef CIBARCODEDESCRIPTOR_H
#define CIBARCODEDESCRIPTOR_H

#ifdef __OBJC__

#import <CoreImage/CoreImageDefines.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// An abstract base class that represents a machine-readable code's attributes.
///
/// Subclasses encapsulate the formal specification and fields specific to a code type. 
/// Each subclass is sufficient to recreate the unique symbol exactly as seen or used with a custom parser.
///
NS_CLASS_AVAILABLE(10_13, 11_0)
@interface CIBarcodeDescriptor : NSObject <NSSecureCoding, NSCopying>
@end

/// Constants indicating the percentage of the symbol that is dedicated to error correction.
typedef CF_ENUM(NSInteger, CIQRCodeErrorCorrectionLevel)
{
    /// Indicates that approximately 20% of the symbol data is dedicated to error correction.
    CIQRCodeErrorCorrectionLevelL NS_SWIFT_NAME(levelL) = 'L',
    
    /// Indicates that approximately 37% of the symbol data is dedicated to error correction.
    CIQRCodeErrorCorrectionLevelM NS_SWIFT_NAME(levelM) = 'M',
    
    /// Indicates that approximately 55% of the symbol data is dedicated to error correction.
    CIQRCodeErrorCorrectionLevelQ NS_SWIFT_NAME(levelQ) = 'Q',
    
    /// Indicates that approximately 65% of the symbol data is dedicated to error correction.
    CIQRCodeErrorCorrectionLevelH NS_SWIFT_NAME(levelH) = 'H',

} NS_SWIFT_NAME(CIQRCodeDescriptor.ErrorCorrectionLevel);


/// A concrete subclass of the Core Image Barcode Descriptor that represents a square QR code symbol.
///
/// ISO/IEC 18004 defines versions from 1 to 40, where a higher symbol version indicates a 
/// larger data-carrying capacity.
/// QR Codes can encode text, vCard contact information, or Uniform Resource Identifiers (URI).
///
NS_CLASS_AVAILABLE(10_13, 11_0)
@interface CIQRCodeDescriptor : CIBarcodeDescriptor
{
    NSData *errorCorrectedPayload;

    NSInteger symbolVersion;
    uint8_t maskPattern;
    CIQRCodeErrorCorrectionLevel errorCorrectionLevel;
}

/// The error-corrected codeword payload that comprises the QR code symbol.
///
/// QR Codes are formally specified in ISO/IEC 18004:2006(E). 
/// Section 6.4.10 "Bitstream to codeword conversion" specifies the set of 8-bit codewords in the symbol 
/// immediately prior to splitting the message into blocks and applying error correction.
/// 
/// During decode, error correction is applied and if successful, the message is re-ordered to the state immediately 
/// following "Bitstream to codeword conversion." 
/// 
/// The `errorCorrectedPayload` corresponds to this sequence of 8-bit codewords.
///
@property (readonly) NSData *errorCorrectedPayload;

/// The version of the QR code which corresponds to the size of the QR code symbol.
///
/// ISO/IEC 18004 defines versions from 1 to 40, where a higher symbol version indicates a larger data-carrying capacity.
/// This field is required in order to properly interpret the error corrected payload.
///
@property (readonly) NSInteger symbolVersion;

/// The data mask pattern for the QR code symbol.
///
/// QR Codes support eight data mask patterns, which are used to avoid large black or large white areas inside the symbol body. 
/// Valid values range from 0 to 7.
///
@property (readonly) uint8_t maskPattern;

/// The error correction level of the QR code symbol.
///
/// QR Codes support four levels of Reed-Solomon error correction.
/// 
/// The possible error correction levels are enumerated in ``CIDataMatrixCodeECCVersion``.
///
@property (readonly) CIQRCodeErrorCorrectionLevel errorCorrectionLevel;

/// Initializes a QR code descriptor for the given payload and parameters.
///
/// - Parameters:
///   - errorCorrectedPayload: The data to encode in the QR code symbol.
///   - symbolVersion: The symbol version, from 1 through 40.
///   - maskPattern: The mask pattern to use in the QR code, from 0 to 7.
///   - errorCorrectionLevel: The QR code's error correction level: L, M, Q, or H.
/// - Returns:
///     An initialized ``CIAztecCodeDescriptor`` instance
///     or `nil` if the parameters are invalid
///
- (nullable instancetype)initWithPayload:(NSData *)errorCorrectedPayload
                           symbolVersion:(NSInteger)symbolVersion
                             maskPattern:(uint8_t)maskPattern
                    errorCorrectionLevel:(CIQRCodeErrorCorrectionLevel)errorCorrectionLevel;

/// Creates a QR code descriptor for the given payload and parameters.
///
/// - Parameters:
///   - errorCorrectedPayload: The data to encode in the QR code symbol.
///   - symbolVersion: The symbol version, from 1 through 40.
///   - maskPattern: The mask pattern to use in the QR code, from 0 to 7.
///   - errorCorrectionLevel: The QR code's error correction level: L, M, Q, or H.
/// - Returns:
///     An autoreleased ``CIAztecCodeDescriptor`` instance
///     or `nil` if the parameters are invalid
///
+ (nullable instancetype)descriptorWithPayload:(NSData *)errorCorrectedPayload
                                 symbolVersion:(NSInteger)symbolVersion
                                   maskPattern:(uint8_t)maskPattern
                          errorCorrectionLevel:(CIQRCodeErrorCorrectionLevel)errorCorrectionLevel;

@end


/// A concrete subclass the Core Image Barcode Descriptor that represents an Aztec code symbol.
/// 
/// An Aztec code symbol is a 2D barcode format defined by the ISO/IEC 24778:2008 standard. 
/// It encodes data in concentric square rings around a central bullseye pattern.
/// 
NS_CLASS_AVAILABLE(10_13, 11_0)
@interface CIAztecCodeDescriptor : CIBarcodeDescriptor
{
    NSData *errorCorrectedPayload;

    BOOL isCompact;
    NSInteger layerCount;
    NSInteger dataCodewordCount;
}

/// The error-corrected payload that comprises the the Aztec code symbol.
/// 
/// Aztec Codes are formally specified in ISO/IEC 24778:2008(E).
/// 
/// The error corrected payload consists of the 6-, 8-, 10-, or 12-bit message codewords produced 
/// at the end of the step described in section 7.3.1.2 "Formation of data codewords", which exists
/// immediately prior to adding error correction. These codewords have dummy bits inserted to ensure
/// that an entire codeword isn't all 0's or all 1's. Clients will need to remove these extra bits
/// as part of interpreting the payload.
///
@property (readonly) NSData *errorCorrectedPayload;

/// A Boolean value telling if the Aztec code is compact.
/// 
/// Compact Aztec symbols use one-fewer ring in the central finder pattern than full-range 
/// Aztec symbols of the same number of data layers.
/// 
@property (readonly) BOOL isCompact;

/// The number of data layers in the Aztec code symbol.
/// 
/// Combined with ``isCompact-property``, the number of data layers determines the number of  
/// modules in the Aztec Code symbol. Valid values range from 1 to 32. Compact symbols can have 
/// up to 4 data layers.
/// 
/// The number of data layers also determines the number of bits in each data codeword of the message
/// carried by the Aztec Code symbol.
/// 
@property (readonly) NSInteger layerCount;

/// The number of non-error-correction codewords carried by the Aztec code symbol.
/// 
/// Used to determine the level of error correction in conjunction with the number of data layers. 
/// Valid values are 1 to 2048. Compact symbols can have up to 64 message codewords.
/// 
/// > Note: this value can exceed the number of message codewords allowed by the number of data 
/// layers in this symbol. In this case, the actual number of message codewords is 1024 fewer than
/// this value and the message payload is to be interpreted in an application-defined manner.
/// 
@property (readonly) NSInteger dataCodewordCount;

/// Initializes an Aztec code descriptor for the given payload and parameters.
///
/// - Parameters:
///   - errorCorrectedPayload: The data to encode in the Aztec code symbol.
///   - isCompact: A Boolean indicating whether or not the Aztec code is compact.
///   - layerCount: The number of layers in the Aztec code, from 1 to 32.
///   - dataCodewordCount: The number of codewords in the Aztec code, from 1 to 2048.
/// - Returns:
///     An initialized ``CIAztecCodeDescriptor`` instance
///     or `nil` if the parameters are invalid
///
- (nullable instancetype)initWithPayload:(NSData *)errorCorrectedPayload
                               isCompact:(BOOL)isCompact
                              layerCount:(NSInteger)layerCount
                       dataCodewordCount:(NSInteger)dataCodewordCount;

/// Creates an Aztec code descriptor for the given payload and parameters.
///
/// - Parameters:
///   - errorCorrectedPayload: The data to encode in the Aztec code symbol.
///   - isCompact: A Boolean indicating whether or not the Aztec code is compact.
///   - layerCount: The number of layers in the Aztec code, from 1 to 32.
///   - dataCodewordCount: The number of codewords in the Aztec code, from 1 to 2048.
/// - Returns:
///     An autoreleased ``CIAztecCodeDescriptor`` instance
///     or `nil` if the parameters are invalid
///
+ (nullable instancetype)descriptorWithPayload:(NSData *)errorCorrectedPayload
                                     isCompact:(BOOL)isCompact
                                    layerCount:(NSInteger)layerCount
                             dataCodewordCount:(NSInteger)dataCodewordCount;

@end


/// A concrete subclass of Core Image Barcode Descriptor that represents a PDF417 symbol.
///
/// PDF417 is a stacked linear barcode symbol format used predominantly in transport, ID cards, 
/// and inventory management. Each pattern in the code comprises 4 bars and spaces, 17 units long.
///
/// Refer to the ISO/IEC 15438:2006(E) for the PDF417 symbol specification.
///
NS_CLASS_AVAILABLE(10_13, 11_0)
@interface CIPDF417CodeDescriptor : CIBarcodeDescriptor
{   
    NSData *errorCorrectedPayload;
    
    BOOL isCompact;
    NSInteger rowCount;
    NSInteger columnCount;
}

/// The error-corrected payload containing the data encoded in the PDF417 code symbol.
///
/// The first codeword indicates the number of data codewords in the errorCorrectedPayload.
///
/// PDF417 codes are comprised of a start character on the left and a stop character on the right. 
/// Each row begins and ends with special characters indicating the current row as well as information
/// about the dimensions of the PDF417 symbol. The errorCorrectedPayload represents the sequence
/// of PDF417 codewords that make up the body of the message. The first codeword indicates the number 
/// of codewords in the message. This count includes the "count" codeword and any padding codewords, 
/// but does not include the error correction codewords. Each codeword is a 16-bit value in the range 
/// of 0...928. The sequence is to be interpreted as described in the PDF417 bar code symbology 
/// specification -- ISO/IEC 15438:2006(E).
///
@property(readonly) NSData *errorCorrectedPayload;

/// A boolean value telling if the PDF417 code is compact.
/// 
/// Compact PDF417 symbols have abbreviated right-side guard bars.
///
@property (readonly) BOOL isCompact;

/// The number of rows in the PDF417 code symbol.
/// 
/// Valid row count values are from 3 to 90.
///
@property (readonly) NSInteger rowCount;

/// The number of columns in the PDF417 code symbol.
///
/// Valid column count values are from 1 to 30.
/// This count excluded the columns used to indicate the symbol structure.
///
@property (readonly) NSInteger columnCount;

/// Initializes an PDF417 code descriptor for the given payload and parameters.
///
/// - Parameters:
///   - errorCorrectedPayload: The data to encode in the PDF417 code symbol.
///   - isCompact: A Boolean indicating whether or not the PDF417 code is compact.
///   - rowCount: The number of rows in the PDF417 code, from 3 to 90.
///   - columnCount: The number of columns in the Aztec code, from 1 to 30.
/// - Returns:
///     An initialized ``CIPDF417CodeDescriptor`` instance
///     or `nil` if the parameters are invalid
///
- (nullable instancetype)initWithPayload:(NSData *)errorCorrectedPayload
                               isCompact:(BOOL)isCompact
                                rowCount:(NSInteger)rowCount
                             columnCount:(NSInteger)columnCount;

/// Creates an PDF417 code descriptor for the given payload and parameters.
///
/// - Parameters:
///   - errorCorrectedPayload: The data to encode in the PDF417 code symbol.
///   - isCompact: A Boolean indicating whether or not the PDF417 code is compact.
///   - rowCount: The number of rows in the PDF417 code, from 3 to 90.
///   - columnCount: The number of columns in the Aztec code, from 1 to 30.
/// - Returns:
///     An autoreleased ``CIPDF417CodeDescriptor`` instance
///     or `nil` if the parameters are invalid
///
+ (nullable instancetype)descriptorWithPayload:(NSData *)errorCorrectedPayload
                                     isCompact:(BOOL)isCompact
                                      rowCount:(NSInteger)rowCount
                                   columnCount:(NSInteger)columnCount;

@end


/// Constants indicating the Data Matrix code ECC version.
///
/// ECC 000 - 140 symbols offer five levels of error correction using convolutional code error correction. 
/// Each successive level or error correction offers more protection for the message data but increases the 
/// size of the symbol required to carry a given message. See the ISO/IEC 16022:2006 spec for other modes.
///
/// ECC 200 symbols utilize Reed-Solomon error correction. 
/// The error correction capacity for any given Data Matrix symbol is fixed by the size (in rows and columns)
/// of the symbol. See Table 7 of ISO/IEC 16022:2006(E) for more details.
///
typedef CF_ENUM(NSInteger, CIDataMatrixCodeECCVersion)
{
    /// Indicates error correction using convolutional code error correction with no data protection.
    CIDataMatrixCodeECCVersion000 NS_SWIFT_NAME(v000) =   0,
    
    /// Indicates 1/4 of the symbol is dedicated to convolutional code error correction.
    CIDataMatrixCodeECCVersion050 NS_SWIFT_NAME(v050) =  50,
    
    /// Indicates 1/3 of the symbol is dedicated to convolutional code error correction.
    CIDataMatrixCodeECCVersion080 NS_SWIFT_NAME(v080) =  80,
    
    /// Indicates 1/2 of the symbol is dedicated to convolutional code error correction.
    CIDataMatrixCodeECCVersion100 NS_SWIFT_NAME(v100) = 100,
    
    /// Indicates 3/4 of the symbol is dedicated to convolutional code error correction.
    CIDataMatrixCodeECCVersion140 NS_SWIFT_NAME(v140) = 140,
    
    /// Indicates error correction using Reed-Solomon error correction. Data protection overhead varies based on symbol size.
    CIDataMatrixCodeECCVersion200 NS_SWIFT_NAME(v200) = 200,

} NS_SWIFT_NAME(CIDataMatrixCodeDescriptor.ECCVersion);


/// A concrete subclass the Core Image Barcode Descriptor that represents an Data Matrix code symbol.
/// 
/// A Data Matrix code symbol is a 2D barcode format defined by the ISO/IEC 16022:2006(E) standard. 
/// It encodes data in square or rectangular symbol with solid lines on the left and bottom sides
///
NS_CLASS_AVAILABLE(10_13, 11_0)
@interface CIDataMatrixCodeDescriptor : CIBarcodeDescriptor
{
    NSData *errorCorrectedPayload;

    NSInteger rowCount;
    NSInteger columnCount;
    CIDataMatrixCodeECCVersion eccVersion;
}

/// The error-corrected payload containing the data encoded in the Data Matrix code symbol.
///
/// DataMatrix symbols are specified bn ISO/IEC 16022:2006(E). ECC 200-type symbols will always 
/// have an even number of rows and columns.
///  
/// For ECC 200-type symbols, the phases of encoding data into a symbol are described in 
/// section 5.1 -- Encode procedure overview. The error corrected payload comprises the 
/// de-interleaved bits of the message described at the end of Step 1: Data encodation.
/// 
@property (readonly) NSData *errorCorrectedPayload;

/// The number of rows in the Data Matrix code symbol.
/// 
/// Refer to ISO/IEC 16022:2006(E) for valid module row and column count combinations.
///
@property (readonly) NSInteger rowCount;

/// The number of columns in the Data Matrix code symbol.
/// 
/// Refer to ISO/IEC 16022:2006(E) for valid module row and column count combinations.
///
@property (readonly) NSInteger columnCount;

/// The error correction version of the Data Matrix code symbol.
/// 
/// The possible error correction version are enumerated in ``CIDataMatrixCodeECCVersion``.
/// Any symbol with an even number of rows and columns will be ECC 200.
///
@property (readonly) CIDataMatrixCodeECCVersion eccVersion;

// Initializes a descriptor that can be used as input to CIBarcodeGenerator
/// Initializes a Data Matrix code descriptor for the given payload and parameters.
///
/// - Parameters:
///   - errorCorrectedPayload: The data to encode in the Data Matrix code symbol.
///   - rowCount: The number of rows in the Data Matrix code symbol.
///   - columnCount: The number of columns in the Data Matrix code symbol.
///   - eccVersion: The ``CIDataMatrixCodeECCVersion`` for the Data Matrix code symbol.
/// - Returns:
///     An initialized ``CIAztecCodeDescriptor`` instance
///     or `nil` if the parameters are invalid
///
- (nullable instancetype)initWithPayload:(NSData *)errorCorrectedPayload
                                rowCount:(NSInteger)rowCount
                             columnCount:(NSInteger)columnCount
                              eccVersion:(CIDataMatrixCodeECCVersion)eccVersion;

/// Creates a Data Matrix code descriptor for the given payload and parameters.
///
/// - Parameters:
///   - errorCorrectedPayload: The data to encode in the Data Matrix code symbol.
///   - rowCount: The number of rows in the Data Matrix code symbol.
///   - columnCount: The number of columns in the Data Matrix code symbol.
///   - eccVersion: The ``CIDataMatrixCodeECCVersion`` for the Data Matrix code symbol.
/// - Returns:
///     An autoreleased ``CIAztecCodeDescriptor`` instance
///     or `nil` if the parameters are invalid
///
+ (nullable instancetype)descriptorWithPayload:(NSData *)errorCorrectedPayload
                                      rowCount:(NSInteger)rowCount
                                   columnCount:(NSInteger)columnCount
                                    eccVersion:(CIDataMatrixCodeECCVersion)eccVersion;

@end


@class NSUserActivity;

@interface NSUserActivity (CIBarcodeDescriptor)

/// The scanned code in the user activity passed in by system scanner.
/// 
/// This property is optional. This value is present if the user activity was created from a source that detected a QR code or other code symbol.
///
@property (nonatomic, nullable, readonly, copy) CIBarcodeDescriptor *detectedBarcodeDescriptor API_AVAILABLE(macos(10.13.4), ios(11.3), tvos(11.3));

@end

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CIBARCODEDESCRIPTOR_H */
