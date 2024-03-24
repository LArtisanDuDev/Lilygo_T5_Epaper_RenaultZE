#include <Arduino.h>

// ==================================
//           CUSTOMIZE BEGIN
// ==================================

// Your myRenault Account Credentials
const String myRenaultLogin = "";
const String myRenaultPassword = "";

// RenaultZE API BEGIN
// use https://github.com/hacf-fr/renault-api to get RenaultZE API configuration data
// $ pip install renault-api[cli]
// $ renault-api --log status
// browse .credentials/renault-api.json to find values

const String gigya_root_url = "";
const String gigya_api_key = "";

const String kamereon_root_url = "";
const String kamereon_api_key = "";

// kept typo from python code : accound_id/account_id
const String accound_id = "";
const String vin = "";
const String country = "";
// RenaultZE API END

// personnal wifi
const char* wifi_ssid = "";
const char* wifi_key = "";

// board wake up interval in seconds
const int WAKEUP_INTERVAL = 600;

// ==================================
//           CUSTOMIZE END
// ==================================