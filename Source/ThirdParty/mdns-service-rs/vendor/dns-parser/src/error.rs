use std::str::Utf8Error;

quick_error! {
    /// Error parsing DNS packet
    #[derive(Debug)]
    pub enum Error {
        /// Invalid compression pointer not pointing backwards
        /// when parsing label
        BadPointer {
            description("invalid compression pointer not pointing backwards \
                         when parsing label")
        }
        /// Packet is smaller than header size
        HeaderTooShort {
            description("packet is smaller than header size")
        }
        /// Packet ihas incomplete data
        UnexpectedEOF {
            description("packet is has incomplete data")
        }
        /// Wrong (too short or too long) size of RDATA
        WrongRdataLength {
            description("wrong (too short or too long) size of RDATA")
        }
        /// Packet has non-zero reserved bits
        ReservedBitsAreNonZero {
            description("packet has non-zero reserved bits")
        }
        /// Label in domain name has unknown label format
        UnknownLabelFormat {
            description("label in domain name has unknown label format")
        }
        /// Query type code is invalid
        InvalidQueryType(code: u16) {
            description("query type code is invalid")
            display("query type {} is invalid", code)
        }
        /// Query class code is invalid
        InvalidQueryClass(code: u16) {
            description("query class code is invalid")
            display("query class {} is invalid", code)
        }
        /// Type code is invalid
        InvalidType(code: u16) {
            description("type code is invalid")
            display("type {} is invalid", code)
        }
        /// Class code is invalid
        InvalidClass(code: u16) {
            description("class code is invalid")
            display("class {} is invalid", code)
        }
        /// Invalid characters encountered while reading label
        LabelIsNotAscii {
            description("invalid characters encountered while reading label")
        }
        /// Invalid characters encountered while reading TXT
        TxtDataIsNotUTF8(error: Utf8Error) {
            description("invalid characters encountered while reading TXT")
            display("{:?}", error)
        }
        /// Parser is in the wrong state
        WrongState {
            description("parser is in the wrong state")
        }
        /// Additional OPT record found
        AdditionalOPT {
            description("additional OPT record found")
        }
    }
}
