use Name;

#[derive(Debug, Clone, Copy)]
pub struct Record<'a>(pub Name<'a>);

impl<'a> ToString for Record<'a> {
    #[inline]
    fn to_string(&self) -> String {
        self.0.to_string()
    }
}

impl<'a> super::Record<'a> for Record<'a> {

    const TYPE: isize = 5;

    fn parse(rdata: &'a [u8], original: &'a [u8]) -> super::RDataResult<'a> {
        let name = Name::scan(rdata, original)?;
        let record = Record(name);
        Ok(super::RData::CNAME(record))
    }
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
    fn parse_response() {
        let response = b"\xfc\x9d\x81\x80\x00\x01\x00\x06\x00\x02\x00\x02\x03\
            cdn\x07sstatic\x03net\x00\x00\x01\x00\x01\xc0\x0c\x00\x05\x00\x01\
            \x00\x00\x00f\x00\x02\xc0\x10\xc0\x10\x00\x01\x00\x01\x00\x00\x00\
            f\x00\x04h\x10g\xcc\xc0\x10\x00\x01\x00\x01\x00\x00\x00f\x00\x04h\
            \x10k\xcc\xc0\x10\x00\x01\x00\x01\x00\x00\x00f\x00\x04h\x10h\xcc\
            \xc0\x10\x00\x01\x00\x01\x00\x00\x00f\x00\x04h\x10j\xcc\xc0\x10\
            \x00\x01\x00\x01\x00\x00\x00f\x00\x04h\x10i\xcc\xc0\x10\x00\x02\
            \x00\x01\x00\x00\x99L\x00\x0b\x08cf-dns02\xc0\x10\xc0\x10\x00\x02\
            \x00\x01\x00\x00\x99L\x00\x0b\x08cf-dns01\xc0\x10\xc0\xa2\x00\x01\
            \x00\x01\x00\x00\x99L\x00\x04\xad\xf5:5\xc0\x8b\x00\x01\x00\x01\x00\
            \x00\x99L\x00\x04\xad\xf5;\x04";

        let packet = Packet::parse(response).unwrap();
        assert_eq!(packet.header, Header {
            id: 64669,
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
            nameservers: 2,
            additional: 2,
        });

        assert_eq!(packet.questions.len(), 1);
        assert_eq!(packet.questions[0].qtype, QT::A);
        assert_eq!(packet.questions[0].qclass, QC::IN);
        assert_eq!(&packet.questions[0].qname.to_string()[..], "cdn.sstatic.net");
        assert_eq!(packet.answers.len(), 6);
        assert_eq!(&packet.answers[0].name.to_string()[..], "cdn.sstatic.net");
        assert_eq!(packet.answers[0].cls, C::IN);
        assert_eq!(packet.answers[0].ttl, 102);
        match packet.answers[0].data {
            RData::CNAME(cname) => {
                assert_eq!(&cname.0.to_string(), "sstatic.net");
            }
            ref x => panic!("Wrong rdata {:?}", x),
        }

        let ips = vec![
            Ipv4Addr::new(104, 16, 103, 204),
            Ipv4Addr::new(104, 16, 107, 204),
            Ipv4Addr::new(104, 16, 104, 204),
            Ipv4Addr::new(104, 16, 106, 204),
            Ipv4Addr::new(104, 16, 105, 204),
        ];
        for i in 1..6 {
            assert_eq!(&packet.answers[i].name.to_string()[..], "sstatic.net");
            assert_eq!(packet.answers[i].cls, C::IN);
            assert_eq!(packet.answers[i].ttl, 102);
            match packet.answers[i].data {
                RData::A(addr) => {
                    assert_eq!(addr.0, ips[i-1]);
                }
                ref x => panic!("Wrong rdata {:?}", x),
            }
        }
    }
}
