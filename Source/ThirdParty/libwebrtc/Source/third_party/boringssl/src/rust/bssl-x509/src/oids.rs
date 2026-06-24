// Copyright 2026 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// 2.5.4.3 - `commonName`
const COMMON_NAME: &[u8] = &[0x55, 0x04, 0x03];

/// 2.5.4.6 - `countryName`
const COUNTRY_NAME: &[u8] = &[0x55, 0x04, 0x06];

/// 2.5.4.7 - `localityName`
const LOCALITY_NAME: &[u8] = &[0x55, 0x04, 0x07];

/// 2.5.4.8 - `stateOrProvinceName`
const STATE_OR_PROVINCE_NAME: &[u8] = &[0x55, 0x04, 0x08];

/// 2.5.4.10 - `organizationName`
const ORGANIZATION_NAME: &[u8] = &[0x55, 0x04, 0x0a];

/// 2.5.4.11 - `organizationalUnitName`
const ORGANIZATIONAL_UNIT_NAME: &[u8] = &[0x55, 0x04, 0x0b];

/// 2.5.4.41 - `name`
const NAME: &[u8] = &[0x55, 0x04, 0x29];

/// 2.5.4.42 - `givenName`
const GIVEN_NAME: &[u8] = &[0x55, 0x04, 0x2a];

/// 2.5.4.4 - `surname`
const SURNAME: &[u8] = &[0x55, 0x04, 0x04];

/// 2.5.4.43 - `initials`
const INITIALS: &[u8] = &[0x55, 0x04, 0x2b];

/// 1.2.840.113549.1.9.20 - `friendlyName`
const FRIENDLY_NAME: &[u8] = &[0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x14];

/// 1.2.840.113549.1.9.1 - `emailAddress`
const EMAIL_ADDRESS: &[u8] = &[0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01];

/// 1.2.840.113549.1.9.2 - `unstructuredName`
const UNSTRUCTURED_NAME: &[u8] = &[0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x02];

/// 1.2.840.113549.1.9.8 - `unstructuredAddress`
const UNSTRUCTURED_ADDRESS: &[u8] = &[0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x08];

/// 2.5.4.46 - `dnQualifier`
const DN_QUALIFIER: &[u8] = &[0x55, 0x04, 0x2e];

/// 0.9.2342.19200300.100.1.25 - `domainComponent`
const DOMAIN_COMPONENT: &[u8] = &[0x09, 0x92, 0x26, 0x89, 0x93, 0xf2, 0x2c, 0x64, 0x01, 0x19];

/// 1.3.6.1.4.1.311.17.1 - `CSPName`
const CSP_NAME: &[u8] = &[0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x11, 0x01];
