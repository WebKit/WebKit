use std::i32;

use byteorder::{BigEndian, ByteOrder};

use {Header, Packet, Error, Question, Name, QueryType, QueryClass};
use {Type, Class, ResourceRecord, RData};
use rdata::opt::Record as Opt;

const OPT_RR_START: [u8; 3] = [0, 0, 41];

impl<'a> Packet<'a> {
    /// Parse a full DNS Packet and return a structure that has all the
    /// data borrowed from the passed buffer.
    pub fn parse(data: &[u8]) -> Result<Packet, Error> {
        let header = try!(Header::parse(data));
        let mut offset = Header::size();
        let mut questions = Vec::with_capacity(header.questions as usize);
        for _ in 0..header.questions {
            let name = try!(Name::scan(&data[offset..], data));
            offset += name.byte_len();
            if offset + 4 > data.len() {
                return Err(Error::UnexpectedEOF);
            }
            let qtype = try!(QueryType::parse(
                BigEndian::read_u16(&data[offset..offset+2])));
            offset += 2;

            let (prefer_unicast, qclass) = try!(parse_qclass_code(
                BigEndian::read_u16(&data[offset..offset+2])));
            offset += 2;

            questions.push(Question {
                qname: name,
                qtype: qtype,
                prefer_unicast: prefer_unicast,
                qclass: qclass,
            });
        }
        let mut answers = Vec::with_capacity(header.answers as usize);
        for _ in 0..header.answers {
            answers.push(try!(parse_record(data, &mut offset)));
        }
        let mut nameservers = Vec::with_capacity(header.nameservers as usize);
        for _ in 0..header.nameservers {
            nameservers.push(try!(parse_record(data, &mut offset)));
        }
        let mut additional = Vec::with_capacity(header.additional as usize);
        let mut opt = None;
        for _ in 0..header.additional {
            if offset + 3 <= data.len() && data[offset..offset+3] == OPT_RR_START {
                if opt.is_none() {
                    opt = Some(try!(parse_opt_record(data, &mut offset)));
                } else {
                    return Err(Error::AdditionalOPT);
                }
            } else {
                additional.push(try!(parse_record(data, &mut offset)));
            }
        }
        Ok(Packet {
            header: header,
            questions: questions,
            answers: answers,
            nameservers: nameservers,
            additional: additional,
            opt: opt,
        })
    }
}

fn parse_qclass_code(value: u16) -> Result<(bool, QueryClass), Error> {
    let prefer_unicast = value & 0x8000 == 0x8000;
    let qclass_code = value & 0x7FFF;

    let qclass = try!(QueryClass::parse(qclass_code));
    Ok((prefer_unicast, qclass))
}

fn parse_class_code(value: u16) -> Result<(bool, Class), Error> {
    let is_unique = value & 0x8000 == 0x8000;
    let class_code = value & 0x7FFF;

    let cls = try!(Class::parse(class_code));
    Ok((is_unique, cls))
}

// Generic function to parse answer, nameservers, and additional records.
fn parse_record<'a>(data: &'a [u8], offset: &mut usize) -> Result<ResourceRecord<'a>, Error> {
    let name = try!(Name::scan(&data[*offset..], data));
    *offset += name.byte_len();
    if *offset + 10 > data.len() {
        return Err(Error::UnexpectedEOF);
    }
    let typ = try!(Type::parse(
        BigEndian::read_u16(&data[*offset..*offset+2])));
    *offset += 2;

    let class_code = BigEndian::read_u16(&data[*offset..*offset+2]);
    let (multicast_unique, cls) = try!(parse_class_code(class_code));
    *offset += 2;

    let mut ttl = BigEndian::read_u32(&data[*offset..*offset+4]);
    if ttl > i32::MAX as u32 {
        ttl = 0;
    }
    *offset += 4;
    let rdlen = BigEndian::read_u16(&data[*offset..*offset+2]) as usize;
    *offset += 2;
    if *offset + rdlen > data.len() {
        return Err(Error::UnexpectedEOF);
    }
    let data = try!(RData::parse(typ,
        &data[*offset..*offset+rdlen], data));
    *offset += rdlen;
    Ok(ResourceRecord {
        name: name,
        multicast_unique: multicast_unique,
        cls: cls,
        ttl: ttl,
        data: data,
    })
}

// Function to parse an RFC 6891 OPT Pseudo RR
fn parse_opt_record<'a>(data: &'a [u8], offset: &mut usize) -> Result<Opt<'a>, Error> {
    if *offset + 11 > data.len() {
        return Err(Error::UnexpectedEOF);
    }
    *offset += 1;
    let typ = try!(Type::parse(
        BigEndian::read_u16(&data[*offset..*offset+2])));
    if typ != Type::OPT {
        return Err(Error::InvalidType(typ as u16));
    }
    *offset += 2;
    let udp = BigEndian::read_u16(&data[*offset..*offset+2]);
    *offset += 2;
    let extrcode = data[*offset];
    *offset += 1;
    let version = data[*offset];
    *offset += 1;
    let flags = BigEndian::read_u16(&data[*offset..*offset+2]);
    *offset += 2;
    let rdlen = BigEndian::read_u16(&data[*offset..*offset+2]) as usize;
    *offset += 2;
    if *offset + rdlen > data.len() {
        return Err(Error::UnexpectedEOF);
    }
    let data = try!(RData::parse(typ,
        &data[*offset..*offset+rdlen], data));
    *offset += rdlen;

    Ok(Opt {
        udp: udp,
        extrcode: extrcode,
        version: version,
        flags: flags,
        data: data,
    })
}

#[cfg(test)]
mod test {

    use std::net::Ipv4Addr;
    use {Packet, Header};
    use Opcode::*;
    use ResponseCode::NoError;
    use QueryType as QT;
    use QueryClass as QC;
    use Class as C;
    use RData;

    #[test]
    fn parse_example_query() {
        let query = b"\x06%\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\
                      \x07example\x03com\x00\x00\x01\x00\x01";
        let packet = Packet::parse(query).unwrap();
        assert_eq!(packet.header, Header {
            id: 1573,
            query: true,
            opcode: StandardQuery,
            authoritative: false,
            truncated: false,
            recursion_desired: true,
            recursion_available: false,
            authenticated_data: false,
            checking_disabled: false,
            response_code: NoError,
            questions: 1,
            answers: 0,
            nameservers: 0,
            additional: 0,
        });
        assert_eq!(packet.questions.len(), 1);
        assert_eq!(packet.questions[0].qtype, QT::A);
        assert_eq!(packet.questions[0].qclass, QC::IN);
        assert_eq!(&packet.questions[0].qname.to_string()[..], "example.com");
        assert_eq!(packet.answers.len(), 0);
    }

    #[test]
    fn parse_example_response() {
        let response = b"\x06%\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00\
                         \x07example\x03com\x00\x00\x01\x00\x01\
                         \xc0\x0c\x00\x01\x00\x01\x00\x00\x04\xf8\
                         \x00\x04]\xb8\xd8\"";
        let packet = Packet::parse(response).unwrap();
        assert_eq!(packet.header, Header {
            id: 1573,
            query: false,
            opcode: StandardQuery,
            authoritative: false,
            truncated: false,
            recursion_desired: true,
            recursion_available: true,
            authenticated_data: false,
            checking_disabled: false,
            response_code: NoError,
            questions: 1,
            answers: 1,
            nameservers: 0,
            additional: 0,
        });
        assert_eq!(packet.questions.len(), 1);
        assert_eq!(packet.questions[0].qtype, QT::A);
        assert_eq!(packet.questions[0].qclass, QC::IN);
        assert_eq!(&packet.questions[0].qname.to_string()[..], "example.com");
        assert_eq!(packet.answers.len(), 1);
        assert_eq!(&packet.answers[0].name.to_string()[..], "example.com");
        assert_eq!(packet.answers[0].multicast_unique, false);
        assert_eq!(packet.answers[0].cls, C::IN);
        assert_eq!(packet.answers[0].ttl, 1272);
        match packet.answers[0].data {
            RData::A(addr) => {
                assert_eq!(addr.0, Ipv4Addr::new(93, 184, 216, 34));
            }
            ref x => panic!("Wrong rdata {:?}", x),
        }
    }

    #[test]
    fn parse_response_with_multicast_unique() {
        let response = b"\x06%\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00\
                         \x07example\x03com\x00\x00\x01\x00\x01\
                         \xc0\x0c\x00\x01\x80\x01\x00\x00\x04\xf8\
                         \x00\x04]\xb8\xd8\"";
        let packet = Packet::parse(response).unwrap();

        assert_eq!(packet.answers.len(), 1);
        assert_eq!(packet.answers[0].multicast_unique, true);
        assert_eq!(packet.answers[0].cls, C::IN);
    }

     #[test]
     fn parse_additional_record_response() {
         let response = b"\x4a\xf0\x81\x80\x00\x01\x00\x01\x00\x01\x00\x01\
                          \x03www\x05skype\x03com\x00\x00\x01\x00\x01\
                          \xc0\x0c\x00\x05\x00\x01\x00\x00\x0e\x10\
                          \x00\x1c\x07\x6c\x69\x76\x65\x63\x6d\x73\x0e\x74\
                          \x72\x61\x66\x66\x69\x63\x6d\x61\x6e\x61\x67\x65\
                          \x72\x03\x6e\x65\x74\x00\
                          \xc0\x42\x00\x02\x00\x01\x00\x01\xd5\xd3\x00\x11\
                          \x01\x67\x0c\x67\x74\x6c\x64\x2d\x73\x65\x72\x76\x65\x72\x73\
                          \xc0\x42\
                          \x01\x61\xc0\x55\x00\x01\x00\x01\x00\x00\xa3\x1c\
                          \x00\x04\xc0\x05\x06\x1e";
          let packet = Packet::parse(response).unwrap();
          assert_eq!(packet.header, Header {
              id: 19184,
              query: false,
              opcode: StandardQuery,
              authoritative: false,
              truncated: false,
              recursion_desired: true,
              recursion_available: true,
              authenticated_data: false,
              checking_disabled: false,
              response_code: NoError,
              questions: 1,
              answers: 1,
              nameservers: 1,
              additional: 1,
          });
          assert_eq!(packet.questions.len(), 1);
          assert_eq!(packet.questions[0].qtype, QT::A);
          assert_eq!(packet.questions[0].qclass, QC::IN);
          assert_eq!(&packet.questions[0].qname.to_string()[..], "www.skype.com");
          assert_eq!(packet.answers.len(), 1);
          assert_eq!(&packet.answers[0].name.to_string()[..], "www.skype.com");
          assert_eq!(packet.answers[0].cls, C::IN);
          assert_eq!(packet.answers[0].ttl, 3600);
          match packet.answers[0].data {
              RData::CNAME(cname) => {
                  assert_eq!(&cname.0.to_string()[..], "livecms.trafficmanager.net");
              }
              ref x => panic!("Wrong rdata {:?}", x),
          }
          assert_eq!(packet.nameservers.len(), 1);
          assert_eq!(&packet.nameservers[0].name.to_string()[..], "net");
          assert_eq!(packet.nameservers[0].cls, C::IN);
          assert_eq!(packet.nameservers[0].ttl, 120275);
          match packet.nameservers[0].data {
              RData::NS(ns) => {
                  assert_eq!(&ns.0.to_string()[..], "g.gtld-servers.net");
              }
              ref x => panic!("Wrong rdata {:?}", x),
          }
          assert_eq!(packet.additional.len(), 1);
          assert_eq!(&packet.additional[0].name.to_string()[..], "a.gtld-servers.net");
          assert_eq!(packet.additional[0].cls, C::IN);
          assert_eq!(packet.additional[0].ttl, 41756);
          match packet.additional[0].data {
              RData::A(addr) => {
                  assert_eq!(addr.0, Ipv4Addr::new(192, 5, 6, 30));
              }
              ref x => panic!("Wrong rdata {:?}", x),
          }
      }

    #[test]
    fn parse_multiple_answers() {
        let response = b"\x9d\xe9\x81\x80\x00\x01\x00\x06\x00\x00\x00\x00\
            \x06google\x03com\x00\x00\x01\x00\x01\xc0\x0c\
            \x00\x01\x00\x01\x00\x00\x00\xef\x00\x04@\xe9\
            \xa4d\xc0\x0c\x00\x01\x00\x01\x00\x00\x00\xef\
            \x00\x04@\xe9\xa4\x8b\xc0\x0c\x00\x01\x00\x01\
            \x00\x00\x00\xef\x00\x04@\xe9\xa4q\xc0\x0c\x00\
            \x01\x00\x01\x00\x00\x00\xef\x00\x04@\xe9\xa4f\
            \xc0\x0c\x00\x01\x00\x01\x00\x00\x00\xef\x00\x04@\
            \xe9\xa4e\xc0\x0c\x00\x01\x00\x01\x00\x00\x00\xef\
            \x00\x04@\xe9\xa4\x8a";
        let packet = Packet::parse(response).unwrap();
        assert_eq!(packet.header, Header {
            id: 40425,
            query: false,
            opcode: StandardQuery,
            authoritative: false,
            truncated: false,
            recursion_desired: true,
            recursion_available: true,
            authenticated_data: false,
            checking_disabled: false,
            response_code: NoError,
            questions: 1,
            answers: 6,
            nameservers: 0,
            additional: 0,
        });
        assert_eq!(packet.questions.len(), 1);
        assert_eq!(packet.questions[0].qtype, QT::A);
        assert_eq!(packet.questions[0].qclass, QC::IN);
        assert_eq!(&packet.questions[0].qname.to_string()[..], "google.com");
        assert_eq!(packet.answers.len(), 6);
        let ips = vec![
            Ipv4Addr::new(64, 233, 164, 100),
            Ipv4Addr::new(64, 233, 164, 139),
            Ipv4Addr::new(64, 233, 164, 113),
            Ipv4Addr::new(64, 233, 164, 102),
            Ipv4Addr::new(64, 233, 164, 101),
            Ipv4Addr::new(64, 233, 164, 138),
        ];
        for i in 0..6 {
            assert_eq!(&packet.answers[i].name.to_string()[..], "google.com");
            assert_eq!(packet.answers[i].cls, C::IN);
            assert_eq!(packet.answers[i].ttl, 239);
            match packet.answers[i].data {
                RData::A(addr) => {
                    assert_eq!(addr.0, ips[i]);
                }
                ref x => panic!("Wrong rdata {:?}", x),
            }
        }
    }

    #[test]
    fn parse_srv_query() {
        let query = b"[\xd9\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\
            \x0c_xmpp-server\x04_tcp\x05gmail\x03com\x00\x00!\x00\x01";
        let packet = Packet::parse(query).unwrap();
        assert_eq!(packet.header, Header {
            id: 23513,
            query: true,
            opcode: StandardQuery,
            authoritative: false,
            truncated: false,
            recursion_desired: true,
            recursion_available: false,
            authenticated_data: false,
            checking_disabled: false,
            response_code: NoError,
            questions: 1,
            answers: 0,
            nameservers: 0,
            additional: 0,
        });
        assert_eq!(packet.questions.len(), 1);
        assert_eq!(packet.questions[0].qtype, QT::SRV);
        assert_eq!(packet.questions[0].qclass, QC::IN);
        assert_eq!(packet.questions[0].prefer_unicast, false);
        assert_eq!(&packet.questions[0].qname.to_string()[..],
            "_xmpp-server._tcp.gmail.com");
        assert_eq!(packet.answers.len(), 0);
    }

    #[test]
    fn parse_multicast_prefer_unicast_query() {
        let query = b"\x06%\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\
                      \x07example\x03com\x00\x00\x01\x80\x01";
        let packet = Packet::parse(query).unwrap();

        assert_eq!(packet.questions.len(), 1);
        assert_eq!(packet.questions[0].qtype, QT::A);
        assert_eq!(packet.questions[0].qclass, QC::IN);
        assert_eq!(packet.questions[0].prefer_unicast, true);
    }

    #[test]
    fn parse_example_query_edns() {
        let query = b"\x95\xce\x01\x00\x00\x01\x00\x00\x00\x00\x00\x01\
            \x06google\x03com\x00\x00\x01\x00\
            \x01\x00\x00\x29\x10\x00\x00\x00\x00\x00\x00\x00";
        let packet = Packet::parse(query).unwrap();
        assert_eq!(packet.header, Header {
            id: 38350,
            query: true,
            opcode: StandardQuery,
            authoritative: false,
            truncated: false,
            recursion_desired: true,
            recursion_available: false,
            authenticated_data: false,
            checking_disabled: false,
            response_code: NoError,
            questions: 1,
            answers: 0,
            nameservers: 0,
            additional: 1,
        });
        assert_eq!(packet.questions.len(), 1);
        assert_eq!(packet.questions[0].qtype, QT::A);
        assert_eq!(packet.questions[0].qclass, QC::IN);
        assert_eq!(&packet.questions[0].qname.to_string()[..], "google.com");
        assert_eq!(packet.answers.len(), 0);
        match packet.opt {
            Some(opt) => {
                assert_eq!(opt.udp, 4096);
                assert_eq!(opt.extrcode, 0);
                assert_eq!(opt.version, 0);
                assert_eq!(opt.flags, 0);
            },
            None => panic!("Missing OPT RR")
        }
    }
}
