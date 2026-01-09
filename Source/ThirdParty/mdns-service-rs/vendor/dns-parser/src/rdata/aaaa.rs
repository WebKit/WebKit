use std::net::Ipv6Addr;

use Error;
use byteorder::{BigEndian, ByteOrder};

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct Record(pub Ipv6Addr);

impl<'a> super::Record<'a> for Record {

    const TYPE: isize = 28;

    fn parse(rdata: &'a [u8], _record: &'a [u8]) -> super::RDataResult<'a> {
        if rdata.len() != 16 {
            return Err(Error::WrongRdataLength);
        }
        let address = Ipv6Addr::new(
            BigEndian::read_u16(&rdata[0..2]),
            BigEndian::read_u16(&rdata[2..4]),
            BigEndian::read_u16(&rdata[4..6]),
            BigEndian::read_u16(&rdata[6..8]),
            BigEndian::read_u16(&rdata[8..10]),
            BigEndian::read_u16(&rdata[10..12]),
            BigEndian::read_u16(&rdata[12..14]),
            BigEndian::read_u16(&rdata[14..16]),
            );
        let record = Record(address);
        Ok(super::RData::AAAA(record))
    }
}

#[cfg(test)]
mod test {

    use {Packet, Header};
    use Opcode::*;
    use ResponseCode::NoError;
    use QueryType as QT;
    use QueryClass as QC;
    use Class as C;
    use RData;
    use super::*;

    #[test]
    fn parse_response() {
        let response = b"\xa9\xd9\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00\x06\
            google\x03com\x00\x00\x1c\x00\x01\xc0\x0c\x00\x1c\x00\x01\x00\x00\
            \x00\x8b\x00\x10*\x00\x14P@\t\x08\x12\x00\x00\x00\x00\x00\x00 \x0e";

        let packet = Packet::parse(response).unwrap();
        assert_eq!(packet.header, Header {
            id: 43481,
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
        assert_eq!(packet.questions[0].qtype, QT::AAAA);
        assert_eq!(packet.questions[0].qclass, QC::IN);
        assert_eq!(&packet.questions[0].qname.to_string()[..], "google.com");
        assert_eq!(packet.answers.len(), 1);
        assert_eq!(&packet.answers[0].name.to_string()[..], "google.com");
        assert_eq!(packet.answers[0].cls, C::IN);
        assert_eq!(packet.answers[0].ttl, 139);
        match packet.answers[0].data {
            RData::AAAA(addr) => {
                assert_eq!(addr.0, Ipv6Addr::new(
                    0x2A00, 0x1450, 0x4009, 0x812, 0, 0, 0, 0x200e)
                );
            }
            ref x => panic!("Wrong rdata {:?}", x),
        }
    }
}
