<?php

/**
 * Download the merchant_id.cer file from developer.apple.com.
 *
 * Convert the DER form certificate to PEM and extract the key by
 * running the following command in Terminal.app:
 *
 * openssl x509 -in merchant_id.cer -inform der -outform pem -out merchant_id.pem
 *
 * Open the Keychain Access.app on macOS and drag the merchant_id.cer file to your
 * login keychain. Right-click to the "Apple Pay Merchant Identity:com.example"
 * certificate and select "Export …". Name the file "merchant_id" and select the
 * "Personal Information Exchange (.p12)" file format. Choose a password to protect
 * the private key, then run the following in Terminal.app:
 *
 * openssl pkcs12 -in merchant_id.p12 -out merchant_id_key.pem -nocerts
 **/

$configFilePath = '/var/www/config/payment-request-config.php';
if (file_exists($configFilePath))
    include($configFilePath);

if (!defined('MERCHANT_IDENTITY_CERTIFICATE'))
    define('MERCHANT_IDENTITY_CERTIFICATE', 'merchant_id.pem');

if (!defined('MERCHANT_IDENTITY_KEY'))
    define('MERCHANT_IDENTITY_KEY', 'merchant_id_key.pem');

if (!defined('MERCHANT_IDENTITY_KEY_PASS'))
    define('MERCHANT_IDENTITY_KEY_PASS', '');

if (!defined('DISPLAY_NAME'))
    define('DISPLAY_NAME', 'WebKit.org Demo');

if (!defined('MERCHANT_IDENTIFIER'))
    define('MERCHANT_IDENTIFIER', openssl_x509_parse(file_get_contents( MERCHANT_IDENTITY_CERTIFICATE ))['subject']['UID'] );

if (!defined('INITIATIVE'))
    define('INITIATIVE', 'web');

if (!defined('INITIATIVE_CONTEXT'))
    define('INITIATIVE_CONTEXT', $_SERVER['HTTP_HOST']);

$postedDataString = file_get_contents('php://input');

try {
    $postedData = json_decode($postedDataString, true);
} catch (Exception $e) {
    die('An error occurred parsing the given data in JSON format: ' . $e->getMessage());
}

$validationURL = isset($postedData['validationURL']) ? $postedData['validationURL'] : '';
$URLcomponents = parse_url($validationURL);
if (!isset($URLcomponents['scheme']) || !isset($URLcomponents['host']))
    die('The validation URL is not valid.');
if ('https' !== strtolower($URLcomponents['scheme']))
    die('The validation URL scheme is not valid.');
$validationHost = strtolower($URLcomponents['host']);
if (!('apple.com' === $validationHost || '.apple.com' === substr($validationHost, -10)))
    die('The validation URL host is not valid.');

$merchantIdentifier = isset($postedData['merchantIdentifier']) ? $postedData['merchantIdentifier'] : MERCHANT_IDENTIFIER;
$displayName = isset($postedData['displayName']) ? $postedData['displayName'] : DISPLAY_NAME;
$intiative = isset($postedData['intiative']) ? $postedData['intiative'] : INITIATIVE;
$intiativeContext = isset($postedData['intiativeContext']) ? $postedData['intiativeContext'] : INITIATIVE_CONTEXT;


$postData = array(
    'merchantIdentifier' => $merchantIdentifier,
    'displayName' => $displayName,
    'domainName' => $intiativeContext,
    'initiative' => $intiative,
    'initiativeContext' => $intiativeContext
);

$postDataFields = json_encode($postData);

$curlOptions = array(
    CURLOPT_URL => $validationURL,
    CURLOPT_POST => true,
    CURLOPT_POSTFIELDS => $postDataFields,
    CURLOPT_RETURNTRANSFER => true,
    CURLOPT_SSLCERT => MERCHANT_IDENTITY_CERTIFICATE,
    CURLOPT_SSLKEY => MERCHANT_IDENTITY_KEY,
    CURLOPT_SSLKEYPASSWD => MERCHANT_IDENTITY_KEY_PASS,
    CURLOPT_SSLKEYPASSWD => MERCHANT_IDENTITY_KEY_PASS,
    CURLOPT_SSL_VERIFYPEER => true
);

$curlConnection = curl_init();
curl_setopt_array($curlConnection, $curlOptions);
if (!$result = curl_exec($curlConnection))
    die('An error occurred when connecting to the validation URL: ' . curl_error($curlConnection));

curl_close($curlConnection);

header('Content-Type: application/json');
echo $result;
exit();