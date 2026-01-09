use {QueryType, QueryClass, Name, Class, Header, RData};
use rdata::opt;


/// Parsed DNS packet
#[derive(Debug)]
#[allow(missing_docs)]  // should be covered by spec
pub struct Packet<'a> {
    pub header: Header,
    pub questions: Vec<Question<'a>>,
    pub answers: Vec<ResourceRecord<'a>>,
    pub nameservers: Vec<ResourceRecord<'a>>,
    pub additional: Vec<ResourceRecord<'a>>,
    /// Optional Pseudo-RR
    /// When present it is sent as an RR in the additional section. In this RR
    /// the `class` and `ttl` fields store max udp packet size and flags
    /// respectively. To keep `ResourceRecord` clean we store the OPT record
    /// here.
    pub opt: Option<opt::Record<'a>>,
}

/// A parsed chunk of data in the Query section of the packet
#[derive(Debug)]
#[allow(missing_docs)]  // should be covered by spec
pub struct Question<'a> {
    pub qname: Name<'a>,
    /// Whether or not we prefer unicast responses.
    /// This is used in multicast DNS.
    pub prefer_unicast: bool,
    pub qtype: QueryType,
    pub qclass: QueryClass,
}

/// A single DNS record
///
/// We aim to provide whole range of DNS records available. But as time is
/// limited we have some types of packets which are parsed and other provided
/// as unparsed slice of bytes.
#[derive(Debug)]
#[allow(missing_docs)]  // should be covered by spec
pub struct ResourceRecord<'a> {
    pub name: Name<'a>,
    /// Whether or not the set of resource records is fully contained in the
    /// packet, or whether there will be more resource records in future
    /// packets. Only used for multicast DNS.
    pub multicast_unique: bool,
    pub cls: Class,
    pub ttl: u32,
    pub data: RData<'a>,
}
