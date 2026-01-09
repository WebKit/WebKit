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

    const TYPE: isize = 12;

    fn parse(rdata: &'a [u8], original: &'a [u8]) -> super::RDataResult<'a> {
        let name = Name::scan(rdata, original)?;
        let record = Record(name);
        Ok(super::RData::PTR(record))
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

    #[test]
    fn parse_response() {
        let response = b"\x53\xd6\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00\
                           \x0269\x0293\x0275\x0272\x07in-addr\x04arpa\x00\
                           \x00\x0c\x00\x01\
                           \xc0\x0c\x00\x0c\x00\x01\x00\x01\x51\x80\x00\x1e\
                           \x10pool-72-75-93-69\x07verizon\x03net\x00";
        let packet = Packet::parse(response).unwrap();
        assert_eq!(packet.header, Header {
            id: 21462,
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
        assert_eq!(packet.questions[0].qtype, QT::PTR);
        assert_eq!(packet.questions[0].qclass, QC::IN);
        assert_eq!(&packet.questions[0].qname.to_string()[..], "69.93.75.72.in-addr.arpa");
        assert_eq!(packet.answers.len(), 1);
        assert_eq!(&packet.answers[0].name.to_string()[..], "69.93.75.72.in-addr.arpa");
        assert_eq!(packet.answers[0].cls, C::IN);
        assert_eq!(packet.answers[0].ttl, 86400);
        match packet.answers[0].data {
            RData::PTR(name) => {
                assert_eq!(&name.0.to_string()[..], "pool-72-75-93-69.verizon.net");
            }
            ref x => panic!("Wrong rdata {:?}", x),
        }
    }
}
