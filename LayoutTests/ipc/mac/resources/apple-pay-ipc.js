// Utilities to facilitate testing WebPaymentCoordinatorProxy::ShowPaymentUI over the IPC testing API.
import { ArgumentSerializer } from "../../coreipc.js";

const contactFields = { postalAddress: false, phone: false, email: false, name: false, phoneticName: false };

const lineItem = {
    type: 0, label: "x", amount: "1.00", paymentTiming: 0,
    recurringPaymentStartDate: { secondsSinceEpochSeconds: 0.0 },
    recurringPaymentIntervalUnit: 0, recurringPaymentIntervalCount: 1,
    recurringPaymentEndDate: { secondsSinceEpochSeconds: 0.0 },
    deferredPaymentDate: { secondsSinceEpochSeconds: 0.0 },
    automaticReloadPaymentThresholdAmount: "",
    disbursementLineItemType: {},
};

const paymentRequest = {
    countryCode: "US",
    currencyCode: "USD",
    requiredBillingContactFields: contactFields,
    billingContact: { pkContact: {} },
    requiredShippingContactFields: contactFields,
    shippingContact: { pkContact: {} },
    supportedNetworks: ["visa"],
    merchantCapabilities: { supports3DS: true, supportsEMV: false, supportsCredit: false, supportsDebit: false, supportsInstantFundsOut: false },
    shippingType: 0,
    shippingMethods: [],
    lineItems: [],
    total: lineItem,
    applicationData: "",
    supportedCountries: [],
    requester: 0,
    installmentConfiguration: { applePayInstallmentConfiguration: {} },
    shippingContactEditingMode: {},
    supportsCouponCode: {},
    couponCode: "",
    recurringPaymentRequest: {},
    automaticReloadPaymentRequest: {},
    multiTokenContexts: {},
    deferredPaymentRequest: {},
    disbursementRequest: {},
    applePayLaterAvailability: {},
    merchantCategoryCode: "",
    isDelegatedRequest: {},
};

export function sendShowPaymentUI()
{
    const messageDefinition = IPC.messages.WebPaymentCoordinatorProxy_ShowPaymentUI;
    const args = {
        destinationID: IPC.pageID,
        webPageProxyIdentifier: IPC.webPageProxyID,
        originatingURL: { string: "https://example.com/" },
        linkIconURLs: [],
        paymentRequest: paymentRequest,
    };
    const serializedArguments = ArgumentSerializer.serializeArguments(messageDefinition.arguments, args);
    return IPC.sendSyncMessage("UI", IPC.pageID, messageDefinition.name, 5000, serializedArguments);
}
