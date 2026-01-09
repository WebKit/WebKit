use {Name, Error};
use byteorder::{BigEndian, ByteOrder};

#[derive(Debug, Clone, Copy)]
pub struct Record<'a> {
    pub preference: u16,
    pub exchange: Name<'a>,
}

impl<'a> super::Record<'a> for Record<'a> {

    const TYPE: isize = 15;

    fn parse(rdata: &'a [u8], original: &'a [u8]) -> super::RDataResult<'a> {
        if rdata.len() < 3 {
            return Err(Error::WrongRdataLength);
        }
        let record = Record {
            preference: BigEndian::read_u16(&rdata[..2]),
            exchange: Name::scan(&rdata[2..], original)?,
        };
        Ok(super::RData::MX(record))
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
        let response = b"\xe3\xe8\x81\x80\x00\x01\x00\x05\x00\x00\x00\x00\
            \x05gmail\x03com\x00\x00\x0f\x00\x01\xc0\x0c\x00\x0f\x00\x01\
            \x00\x00\x04|\x00\x1b\x00\x05\rgmail-smtp-in\x01l\x06google\xc0\
            \x12\xc0\x0c\x00\x0f\x00\x01\x00\x00\x04|\x00\t\x00\
            \n\x04alt1\xc0)\xc0\x0c\x00\x0f\x00\x01\x00\x00\x04|\
            \x00\t\x00(\x04alt4\xc0)\xc0\x0c\x00\x0f\x00\x01\x00\
            \x00\x04|\x00\t\x00\x14\x04alt2\xc0)\xc0\x0c\x00\x0f\
            \x00\x01\x00\x00\x04|\x00\t\x00\x1e\x04alt3\xc0)";
        let packet = Packet::parse(response).unwrap();
        assert_eq!(packet.header, Header {
            id: 58344,
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
        assert_eq!(packet.questions[0].qtype, QT::MX);
        assert_eq!(packet.questions[0].qclass, QC::IN);
        assert_eq!(&packet.questions[0].qname.to_string()[..],
        "gmail.com");
        assert_eq!(packet.answers.len(), 5);
        let items = vec![
            ( 5, "gmail-smtp-in.l.google.com"),
            (10, "alt1.gmail-smtp-in.l.google.com"),
            (40, "alt4.gmail-smtp-in.l.google.com"),
            (20, "alt2.gmail-smtp-in.l.google.com"),
            (30, "alt3.gmail-smtp-in.l.google.com"),
        ];
        for i in 0..5 {
            assert_eq!(&packet.answers[i].name.to_string()[..],
            "gmail.com");
            assert_eq!(packet.answers[i].cls, C::IN);
            assert_eq!(packet.answers[i].ttl, 1148);
            match *&packet.answers[i].data {
                RData::MX( Record { preference, exchange }) => {
                    assert_eq!(preference, items[i].0);
                    assert_eq!(exchange.to_string(), (items[i].1).to_string());
                }
                ref x => panic!("Wrong rdata {:?}", x),
            }
        }
    }
}
