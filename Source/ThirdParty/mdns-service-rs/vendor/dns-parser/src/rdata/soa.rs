use {Name, Error};
use byteorder::{BigEndian, ByteOrder};

/// The SOA (Start of Authority) record
#[derive(Debug, Clone, Copy)]
pub struct Record<'a> {
    pub primary_ns: Name<'a>,
    pub mailbox: Name<'a>,
    pub serial: u32,
    pub refresh: u32,
    pub retry: u32,
    pub expire: u32,
    pub minimum_ttl: u32,
}

impl<'a> super::Record<'a> for Record<'a> {

    const TYPE: isize = 6;

    fn parse(rdata: &'a [u8], original: &'a [u8]) -> super::RDataResult<'a> {
        let mut pos = 0;
        let primary_name_server = try!(Name::scan(rdata, original));
        pos += primary_name_server.byte_len();
        let mailbox = try!(Name::scan(&rdata[pos..], original));
        pos += mailbox.byte_len();
        if rdata[pos..].len() < 20 {
            return Err(Error::WrongRdataLength);
        }
        let record = Record {
            primary_ns: primary_name_server,
            mailbox: mailbox,
            serial: BigEndian::read_u32(&rdata[pos..(pos+4)]),
            refresh: BigEndian::read_u32(&rdata[(pos+4)..(pos+8)]),
            retry: BigEndian::read_u32(&rdata[(pos+8)..(pos+12)]),
            expire: BigEndian::read_u32(&rdata[(pos+12)..(pos+16)]),
            minimum_ttl: BigEndian::read_u32(&rdata[(pos+16)..(pos+20)]),
        };
        Ok(super::RData::SOA(record))
    }
}

#[cfg(test)]
mod test {

    use {Packet, Header};
    use Opcode::*;
    use ResponseCode::NameError;
    use QueryType as QT;
    use QueryClass as QC;
    use Class as C;
    use RData;

     #[test]
     fn parse_response() {
         let response = b"\x9f\xc5\x85\x83\x00\x01\x00\x00\x00\x01\x00\x00\
                          \x0edlkfjkdjdslfkj\x07youtube\x03com\x00\x00\x01\x00\x01\
                          \xc0\x1b\x00\x06\x00\x01\x00\x00\x2a\x30\x00\x1e\xc0\x1b\
                          \x05admin\xc0\x1b\x77\xed\x2a\x73\x00\x00\x51\x80\x00\x00\
                          \x0e\x10\x00\x00\x3a\x80\x00\x00\x2a\x30";
          let packet = Packet::parse(response).unwrap();
          assert_eq!(packet.header, Header {
              id: 40901,
              query: false,
              opcode: StandardQuery,
              authoritative: true,
              truncated: false,
              recursion_desired: true,
              recursion_available: true,
              authenticated_data: false,
              checking_disabled: false,
              response_code: NameError,
              questions: 1,
              answers: 0,
              nameservers: 1,
              additional: 0,
          });
          assert_eq!(packet.questions.len(), 1);
          assert_eq!(packet.questions[0].qtype, QT::A);
          assert_eq!(packet.questions[0].qclass, QC::IN);
          assert_eq!(&packet.questions[0].qname.to_string()[..], "dlkfjkdjdslfkj.youtube.com");
          assert_eq!(packet.answers.len(), 0);

          assert_eq!(packet.nameservers.len(), 1);
          assert_eq!(&packet.nameservers[0].name.to_string()[..], "youtube.com");
          assert_eq!(packet.nameservers[0].cls, C::IN);
          assert_eq!(packet.nameservers[0].multicast_unique, false);
          assert_eq!(packet.nameservers[0].ttl, 10800);
          match packet.nameservers[0].data {
              RData::SOA(ref soa_rec) => {
                assert_eq!(&soa_rec.primary_ns.to_string()[..], "youtube.com");
                assert_eq!(&soa_rec.mailbox.to_string()[..], "admin.youtube.com");
                assert_eq!(soa_rec.serial, 2012031603);
                assert_eq!(soa_rec.refresh, 20864);
                assert_eq!(soa_rec.retry, 3600);
                assert_eq!(soa_rec.expire, 14976);
                assert_eq!(soa_rec.minimum_ttl, 10800);
              }
              ref x => panic!("Wrong rdata {:?}", x),
          }
      }
}
