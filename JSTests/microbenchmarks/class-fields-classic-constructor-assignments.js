class Classic_Constructor_Assignments {
    constructor() {
        this.verification=1;

        this._00=1; this._01=1; this._02=1; this._03=1; this._04=1; this._05=1; this._06=1; this._07=1; this._08=1; this._09=1;
        this._10=1; this._11=1; this._12=1; this._13=1; this._14=1; this._15=1; this._16=1; this._17=1; this._18=1; this._19=1;
        this._20=1; this._21=1; this._22=1; this._23=1; this._24=1; this._25=1; this._26=1; this._27=1; this._28=1; this._29=1;
        this._30=1; this._31=1; this._32=1; this._33=1; this._34=1; this._35=1; this._36=1; this._37=1; this._38=1; this._39=1;
        this._40=1; this._41=1; this._42=1; this._43=1; this._44=1; this._45=1; this._46=1; this._47=1; this._48=1; this._49=1;
        this._50=1; this._51=1; this._52=1; this._53=1; this._54=1; this._55=1; this._56=1; this._57=1; this._58=1; this._59=1;
        this._60=1; this._61=1; this._62=1; this._63=1; this._64=1; this._65=1; this._66=1; this._67=1; this._68=1; this._69=1;
        this._70=1; this._71=1; this._72=1; this._73=1; this._74=1; this._75=1; this._76=1; this._77=1; this._78=1; this._79=1;
        this._80=1; this._81=1; this._82=1; this._83=1; this._84=1; this._85=1; this._86=1; this._87=1; this._88=1; this._89=1;
        this._90=1; this._91=1; this._92=1; this._93=1; this._94=1; this._95=1; this._96=1; this._97=1; this._98=1; this._99=1;
    }
}

const ITERATIONS = 100_000;

function bench(testClass) {
    var acc = 0;
    for (var i=0; i<ITERATIONS; i++) {
        const instance = new testClass()
        acc += instance.verification;
    }
}

bench(Classic_Constructor_Assignments);
