(() => {
    applePayRequestBase = () => {
        return {
            merchantCapabilities: ['supports3DS'],
            supportedNetworks: ['visa'],
            countryCode: 'US',
        };
    };

    applePayPaymentRequest = () => {
        const request = applePayRequestBase();
        request.total = { label: 'total', amount: '0.00' };
        request.currencyCode = 'USD';
        return request;
     };

    applePayMethod = () => {
        const request = applePayRequestBase();
        request.version = 1;
        request.merchantIdentifier = '';
        return {
            supportedMethods: 'https://apple.com/apple-pay',
            data: request,
        };
    };

    applePayDetails = {
        total: {
            label: 'total',
            amount: { currency: 'USD', value: '0.00' },
        },
    };
})();
