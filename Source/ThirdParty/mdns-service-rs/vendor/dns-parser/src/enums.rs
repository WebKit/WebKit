use {Error};
use rdata::Record;
use rdata::*;

/// The TYPE value according to RFC 1035
///
/// All "EXPERIMENTAL" markers here are from the RFC
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum Type {
    /// a host addresss
    A = a::Record::TYPE,
    /// an authoritative name server
    NS = ns::Record::TYPE,
    /// a mail forwarder (Obsolete - use MX)
    MF = mf::Record::TYPE,
    /// the canonical name for an alias
    CNAME = cname::Record::TYPE,
    /// marks the start of a zone of authority
    SOA = soa::Record::TYPE,
    /// a mailbox domain name (EXPERIMENTAL)
    MB = mb::Record::TYPE,
    /// a mail group member (EXPERIMENTAL)
    MG = mg::Record::TYPE,
    /// a mail rename domain name (EXPERIMENTAL)
    MR = mr::Record::TYPE,
    /// a null RR (EXPERIMENTAL)
    NULL = null::Record::TYPE,
    /// a well known service description
    WKS = wks::Record::TYPE,
    /// a domain name pointer
    PTR = ptr::Record::TYPE,
    /// host information
    HINFO = hinfo::Record::TYPE,
    /// mailbox or mail list information
    MINFO = minfo::Record::TYPE,
    /// mail exchange
    MX = mx::Record::TYPE,
    /// text strings
    TXT = txt::Record::TYPE,
    /// IPv6 host address (RFC 2782)
    AAAA = aaaa::Record::TYPE,
    /// service record (RFC 2782)
    SRV = srv::Record::TYPE,
    /// EDNS0 options (RFC 6891)
    OPT = opt::Record::TYPE,
    /// next secure record (RFC 4034, RFC 6762)
    NSEC = nsec::Record::TYPE,
}

/// The QTYPE value according to RFC 1035
///
/// All "EXPERIMENTAL" markers here are from the RFC
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
#[cfg_attr(feature = "with-serde", derive(Serialize, Deserialize))]
pub enum QueryType {
    /// a host addresss
    A = a::Record::TYPE,
    /// an authoritative name server
    NS = ns::Record::TYPE,
    /// a mail forwarder (Obsolete - use MX)
    MF = mf::Record::TYPE,
    /// the canonical name for an alias
    CNAME = cname::Record::TYPE,
    /// marks the start of a zone of authority
    SOA = soa::Record::TYPE,
    /// a mailbox domain name (EXPERIMENTAL)
    MB = mb::Record::TYPE,
    /// a mail group member (EXPERIMENTAL)
    MG = mg::Record::TYPE,
    /// a mail rename domain name (EXPERIMENTAL)
    MR = mr::Record::TYPE,
    /// a null RR (EXPERIMENTAL)
    NULL = null::Record::TYPE,
    /// a well known service description
    WKS = wks::Record::TYPE,
    /// a domain name pointer
    PTR = ptr::Record::TYPE,
    /// host information
    HINFO = hinfo::Record::TYPE,
    /// mailbox or mail list information
    MINFO = minfo::Record::TYPE,
    /// mail exchange
    MX = mx::Record::TYPE,
    /// text strings
    TXT = txt::Record::TYPE,
    /// IPv6 host address (RFC 2782)
    AAAA = aaaa::Record::TYPE,
    /// service record (RFC 2782)
    SRV = srv::Record::TYPE,
    /// A request for a transfer of an entire zone
    AXFR = axfr::Record::TYPE,
    /// A request for mailbox-related records (MB, MG or MR)
    MAILB = mailb::Record::TYPE,
    /// A request for mail agent RRs (Obsolete - see MX)
    MAILA = maila::Record::TYPE,
    /// A request for all records
    All = all::Record::TYPE,
}


/// The CLASS value according to RFC 1035
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum Class {
    /// the Internet
    IN = 1,
    /// the CSNET class (Obsolete - used only for examples in some obsolete
    /// RFCs)
    CS = 2,
    /// the CHAOS class
    CH = 3,
    /// Hesiod [Dyer 87]
    HS = 4,
}

/// The QCLASS value according to RFC 1035
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum QueryClass {
    /// the Internet
    IN = 1,
    /// the CSNET class (Obsolete - used only for examples in some obsolete
    /// RFCs)
    CS = 2,
    /// the CHAOS class
    CH = 3,
    /// Hesiod [Dyer 87]
    HS = 4,
    /// Any class
    Any = 255,
}

/// The OPCODE value according to RFC 1035
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum Opcode {
    /// Normal query
    StandardQuery,
    /// Inverse query (query a name by IP)
    InverseQuery,
    /// Server status request
    ServerStatusRequest,
    /// Reserved opcode for future use
    Reserved(u16),
}

quick_error! {
    /// The RCODE value according to RFC 1035
    #[derive(Debug, PartialEq, Eq, Clone, Copy)]
    #[allow(missing_docs)] // names are from spec
    pub enum ResponseCode {
        NoError
        FormatError
        ServerFailure
        NameError
        NotImplemented
        Refused
        Reserved(code: u8)
    }
}

impl From<u16> for Opcode {
    fn from(code: u16) -> Opcode {
        use self::Opcode::*;
        match code {
            0 => StandardQuery,
            1 => InverseQuery,
            2 => ServerStatusRequest,
            x => Reserved(x),
        }
    }
}
impl Into<u16> for Opcode {
    fn into(self) -> u16 {
        use self::Opcode::*;
        match self {
            StandardQuery => 0,
            InverseQuery => 1,
            ServerStatusRequest => 2,
            Reserved(x) => x,
        }
    }
}

impl From<u8> for ResponseCode {
    fn from(code: u8) -> ResponseCode {
        use self::ResponseCode::*;
        match code {
            0       => NoError,
            1       => FormatError,
            2       => ServerFailure,
            3       => NameError,
            4       => NotImplemented,
            5       => Refused,
            6...15  => Reserved(code),
            x       => panic!("Invalid response code {}", x),
        }
    }
}
impl Into<u8> for ResponseCode {
    fn into(self) -> u8 {
        use self::ResponseCode::*;
        match self {
            NoError         => 0,
            FormatError     => 1,
            ServerFailure   => 2,
            NameError       => 3,
            NotImplemented  => 4,
            Refused         => 5,
            Reserved(code)  => code,
        }
    }
}

impl QueryType {
    /// Parse a query type code
    pub fn parse(code: u16) -> Result<QueryType, Error> {
        use self::QueryType::*;
        match code as isize {
            a::Record::TYPE         => Ok(A),
            ns::Record::TYPE        => Ok(NS),
            mf::Record::TYPE        => Ok(MF),
            cname::Record::TYPE     => Ok(CNAME),
            soa::Record::TYPE       => Ok(SOA),
            mb::Record::TYPE        => Ok(MB),
            mg::Record::TYPE        => Ok(MG),
            mr::Record::TYPE        => Ok(MR),
            null::Record::TYPE      => Ok(NULL),
            wks::Record::TYPE       => Ok(WKS),
            ptr::Record::TYPE       => Ok(PTR),
            hinfo::Record::TYPE     => Ok(HINFO),
            minfo::Record::TYPE     => Ok(MINFO),
            mx::Record::TYPE        => Ok(MX),
            txt::Record::TYPE       => Ok(TXT),
            aaaa::Record::TYPE      => Ok(AAAA),
            srv::Record::TYPE       => Ok(SRV),
            axfr::Record::TYPE      => Ok(AXFR),
            mailb::Record::TYPE     => Ok(MAILB),
            maila::Record::TYPE     => Ok(MAILA),
            all::Record::TYPE       => Ok(All),
            x               => Err(Error::InvalidQueryType(x as u16)),
        }
    }
}

impl QueryClass {
    /// Parse a query class code
    pub fn parse(code: u16) -> Result<QueryClass, Error> {
        use self::QueryClass::*;
        match code {
            1   => Ok(IN),
            2   => Ok(CS),
            3   => Ok(CH),
            4   => Ok(HS),
            255 => Ok(Any),
            x   => Err(Error::InvalidQueryClass(x)),
        }
    }
}

impl Type {
    /// Parse a type code
    pub fn parse(code: u16) -> Result<Type, Error> {
        use self::Type::*;
        match code as isize {
            a::Record::TYPE         => Ok(A),
            ns::Record::TYPE        => Ok(NS),
            mf::Record::TYPE        => Ok(MF),
            cname::Record::TYPE     => Ok(CNAME),
            soa::Record::TYPE       => Ok(SOA),
            mb::Record::TYPE        => Ok(MB),
            mg::Record::TYPE        => Ok(MG),
            mr::Record::TYPE        => Ok(MR),
            null::Record::TYPE      => Ok(NULL),
            wks::Record::TYPE       => Ok(WKS),
            ptr::Record::TYPE       => Ok(PTR),
            hinfo::Record::TYPE     => Ok(HINFO),
            minfo::Record::TYPE     => Ok(MINFO),
            mx::Record::TYPE        => Ok(MX),
            txt::Record::TYPE       => Ok(TXT),
            aaaa::Record::TYPE      => Ok(AAAA),
            srv::Record::TYPE       => Ok(SRV),
            opt::Record::TYPE       => Ok(OPT),
            nsec::Record::TYPE      => Ok(NSEC),
            x               => Err(Error::InvalidType(x as u16)),
        }
    }
}

impl Class {
    /// Parse a class code
    pub fn parse(code: u16) -> Result<Class, Error> {
        use self::Class::*;
        match code {
            1   => Ok(IN),
            2   => Ok(CS),
            3   => Ok(CH),
            4   => Ok(HS),
            x   => Err(Error::InvalidClass(x)),
        }
    }
}
