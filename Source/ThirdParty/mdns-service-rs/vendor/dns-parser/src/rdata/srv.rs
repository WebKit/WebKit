use {Name, Error};
use byteorder::{BigEndian, ByteOrder};

#[derive(Debug, Clone, Copy)]
pub struct Record<'a> {
    pub priority: u16,
    pub weight: u16,
    pub port: u16,
    pub target: Name<'a>,
}

impl<'a> super::Record<'a> for Record<'a> {

    const TYPE: isize = 33;

    fn parse(rdata: &'a [u8], original: &'a [u8]) -> super::RDataResult<'a> {
        if rdata.len() < 7 {
            return Err(Error::WrongRdataLength);
        }
        let record = Record {
            priority: BigEndian::read_u16(&rdata[..2]),
            weight: BigEndian::read_u16(&rdata[2..4]),
            port: BigEndian::read_u16(&rdata[4..6]),
            target: Name::scan(&rdata[6..], original)?,
        };
        Ok(super::RData::SRV(record))
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
        let response = b"[\xd9\x81\x80\x00\x01\x00\x05\x00\x00\x00\x00\
            \x0c_xmpp-server\x04_tcp\x05gmail\x03com\x00\x00!\x00\x01\
            \xc0\x0c\x00!\x00\x01\x00\x00\x03\x84\x00 \x00\x05\x00\x00\
            \x14\x95\x0bxmpp-server\x01l\x06google\x03com\x00\xc0\x0c\x00!\
            \x00\x01\x00\x00\x03\x84\x00%\x00\x14\x00\x00\x14\x95\
            \x04alt3\x0bxmpp-server\x01l\x06google\x03com\x00\
            \xc0\x0c\x00!\x00\x01\x00\x00\x03\x84\x00%\x00\x14\x00\x00\
            \x14\x95\x04alt1\x0bxmpp-server\x01l\x06google\x03com\x00\
            \xc0\x0c\x00!\x00\x01\x00\x00\x03\x84\x00%\x00\x14\x00\x00\
            \x14\x95\x04alt2\x0bxmpp-server\x01l\x06google\x03com\x00\
            \xc0\x0c\x00!\x00\x01\x00\x00\x03\x84\x00%\x00\x14\x00\x00\
            \x14\x95\x04alt4\x0bxmpp-server\x01l\x06google\x03com\x00";
        let packet = Packet::parse(response).unwrap();
        assert_eq!(packet.header, Header {
            id: 23513,
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
            answers: 5,
            nameservers: 0,
            additional: 0,
        });
        assert_eq!(packet.questions.len(), 1);
        assert_eq!(packet.questions[0].qtype, QT::SRV);
        assert_eq!(packet.questions[0].qclass, QC::IN);
        assert_eq!(&packet.questions[0].qname.to_string()[..],
            "_xmpp-server._tcp.gmail.com");
        assert_eq!(packet.answers.len(), 5);
        let items = vec![
            (5, 0, 5269, "xmpp-server.l.google.com"),
            (20, 0, 5269, "alt3.xmpp-server.l.google.com"),
            (20, 0, 5269, "alt1.xmpp-server.l.google.com"),
            (20, 0, 5269, "alt2.xmpp-server.l.google.com"),
            (20, 0, 5269, "alt4.xmpp-server.l.google.com"),
        ];
        for i in 0..5 {
            assert_eq!(&packet.answers[i].name.to_string()[..],
                "_xmpp-server._tcp.gmail.com");
            assert_eq!(packet.answers[i].cls, C::IN);
            assert_eq!(packet.answers[i].ttl, 900);
            match *&packet.answers[i].data {
                RData::SRV(Record { priority, weight, port, target }) => {
                    assert_eq!(priority, items[i].0);
                    assert_eq!(weight, items[i].1);
                    assert_eq!(port, items[i].2);
                    assert_eq!(target.to_string(), (items[i].3).to_string());
                }
                ref x => panic!("Wrong rdata {:?}", x),
            }
        }
    }
}
