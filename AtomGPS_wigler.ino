#include "M5Atom.h"
#include <SPI.h>
#include "FS.h"
#include "SD.h"
#include <WiFi.h>
#include <TinyGPS++.h>
#include <WiFiClientSecure.h>

TinyGPSPlus gps;
String fileName;

const int maxMACs = 75;
String macAddressArray[maxMACs];
int macArrayIndex = 0;

// Auto-upload config, read from /wigle.cfg on the SD card
const int maxKnownNets = 10;
String knownSSID[maxKnownNets];
String knownPass[maxKnownNets];
int knownCount = 0;
String wigleAuth;  // "Encoded for use" token from wigle.net/account
bool wigleDonate = false;

const unsigned long uploadRetryMs = 10UL * 60 * 1000;
unsigned long lastUploadAttempt = 0;
bool uploadDone = false;  // re-armed once no known network is in range

// Root of the api.wigle.net chain (Let's Encrypt). Valid until 2035-06-04.
const char* ISRG_ROOT_X1 =
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  M5.begin(true, false, true);
  SPI.begin(23, 33, 19, -1);

  unsigned long startMillis = millis();
  const unsigned long blinkInterval = 500;
  bool ledState = false;

  if (!SD.begin(-1, SPI, 40000000)) {
    Serial.println("SD Card initialization failed!");
    while (millis() - startMillis < 5000) {  // Continue blinking for 5 seconds
      if (millis() - startMillis > blinkInterval) {
        startMillis = millis();
        ledState = !ledState;
        M5.dis.drawpix(0, ledState ? 0xff0000 : 0x000000);  // Toggle red LED
      }
    }
    return;
  }

  M5.dis.clear();  // Clear LED after blinking
  Serial.println("SD Card initialized.");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("WiFi initialized.");

  Serial1.begin(9600, SERIAL_8N1, 22, -1);
  Serial.println("GPS Serial initialized.");

  loadConfig();
  // At home there's often no GPS fix, so try uploading before blocking on one.
  maybeUpload(WiFi.scanNetworks(false, true, false, 110));

  waitForGPSFix();

  initializeFile();
}

void waitForGPSFix() {
  unsigned long lastBlink = 0;
  const unsigned long blinkInterval = 300;  // Time interval for LED blinking
  bool ledState = false;

  Serial.println("Waiting for GPS fix...");
  while (!gps.location.isValid()) {
    if (Serial1.available() > 0) {
      gps.encode(Serial1.read());
    }

    // Non-blocking LED blink
    if (millis() - lastBlink >= blinkInterval) {
      lastBlink = millis();
      ledState = !ledState;
      M5.dis.drawpix(0, ledState ? 0x800080 : 0x000000);  // Toggle purple LED
    }
  }

  // Green LED indicates GPS fix
  M5.dis.drawpix(0, 0x00ff00);
  delay(200);  // Short delay to show green LED
  M5.dis.clear();
  Serial.println("GPS fix obtained.");
}

void initializeFile() {
  int fileNumber = 0;
  bool isNewFile = false;

  // create a date stamp for the filename
  char fileDateStamp[16];
  sprintf(fileDateStamp, "%04d-%02d-%02d-",
          gps.date.year(), gps.date.month(), gps.date.day());

  do {
    fileName = "/wifi-scans-" + String(fileDateStamp) + String(fileNumber) + ".csv";
    isNewFile = !SD.exists(fileName) && !SD.exists("/uploaded" + fileName);
    fileNumber++;
  } while (!isNewFile);

  if (isNewFile) {
    File dataFile = SD.open(fileName, FILE_WRITE);
    if (dataFile) {
      dataFile.println("WigleWifi-1.4,appRelease=1.300000,model=GPS Kit,release=1.100000F+00,device=M5ATOM,display=NONE,board=ESP32,brand=M5");
      dataFile.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
      dataFile.close();
      Serial.println("New file created: " + fileName);
    }
  } else {
    Serial.println("Using existing file: " + fileName);
  }
}

void loop() {
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  if (gps.location.isValid()) {
    M5.dis.drawpix(0, 0x00ff00);
    delay(180);  // scan delay 
    M5.dis.clear();

    float lat = gps.location.lat();
    float lon = gps.location.lng();
    float altitude = gps.altitude.meters();
    float accuracy = gps.hdop.hdop();

    char utc[20];
    sprintf(utc, "%04d-%02d-%02d %02d:%02d:%02d",
            gps.date.year(), gps.date.month(), gps.date.day(),
            gps.time.hour(), gps.time.minute(), gps.time.second());
    //scanNetworks(bool async, bool show_hidden, bool passive, uint32_t max_ms_per_chan)
    int numNetworks = WiFi.scanNetworks(false, true, false, 110);  //credit J.Hewitt
    for (int i = 0; i < numNetworks; i++) {
      String currentMAC = WiFi.BSSIDstr(i);
      if (!isMACSeen(currentMAC)) {
        macAddressArray[macArrayIndex++] = currentMAC;
        if (macArrayIndex >= maxMACs) macArrayIndex = 0;

        String ssid = "\"" + WiFi.SSID(i) + "\"";  //sanitize SSID
        String capabilities = getAuthType(WiFi.encryptionType(i));
        int channel = WiFi.channel(i);
        int rssi = WiFi.RSSI(i);

        String dataString = currentMAC + "," + ssid + "," + capabilities + "," + utc + "," + String(channel) + "," + String(rssi) + "," + String(lat, 6) + "," + String(lon, 6) + "," + String(altitude, 2) + "," + String(accuracy, 2) + ",WIFI";

        logData(dataString);
      }
    }
    maybeUpload(numNetworks);
    if (!SD.exists(fileName)) initializeFile();  // current file was uploaded and moved
  } else {
    M5.dis.drawpix(0, 0x800080);  // Purple LED if waiting for GPS fix
    delay(500);
    M5.dis.clear();  // Clear LED after waiting
    delay(500);
  }
}

bool isMACSeen(const String& mac) {
  for (int i = 0; i < macArrayIndex; i++) {
    if (macAddressArray[i] == mac) {
      return true;
    }
  }
  return false;
}

void logData(const String& data) {
  File dataFile = SD.open(fileName, FILE_APPEND);
  if (dataFile) {
    dataFile.println(data);
    dataFile.close();
    // M5.dis.drawpix(0, 0x0000ff); // Blue LED for successful write
    // delay(150);
    // M5.dis.clear();
    // Serial.println("Data written: " + data);
  } else {
    Serial.println("Error opening " + fileName);
    M5.dis.drawpix(0, 0xff0000);
    delay(500);
    M5.dis.clear();  // Red flash for file write error
    delay(500);
  }
}

// /wigle.cfg: key=value lines, '#' comments. Each "pass=" belongs to the "ssid=" above it.
void loadConfig() {
  SD.mkdir("/uploaded");
  if (!SD.exists("/wigle.cfg")) {
    File t = SD.open("/wigle.cfg", FILE_WRITE);
    if (t) {
      t.print("# AtomGPS wigler auto-upload config\n"
              "# One ssid=/pass= pair per known network (max 10). Each pass= belongs to the ssid= above it.\n"
              "# Leave pass= empty for open networks. Blank ssid= lines are ignored.\n"
              "ssid=\npass=\n"
              "ssid=\npass=\n"
              "ssid=\npass=\n"
              "# \"Encoded for use\" API token from https://wigle.net/account\n"
              "wigle_auth=\n"
              "# on = allow commercial use of your uploads\n"
              "donate=off\n");
      t.close();
      Serial.println("Created template /wigle.cfg, fill it in to enable auto-upload.");
    }
  }
  File f = SD.open("/wigle.cfg");
  if (!f) {
    Serial.println("No /wigle.cfg, auto-upload disabled.");
    return;
  }
  bool inEntry = false;  // a pass= only counts right after a non-empty ssid=
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    int eq = line.indexOf('=');
    if (line.startsWith("#") || eq < 0) continue;
    String key = line.substring(0, eq);
    String val = line.substring(eq + 1);
    key.trim();
    val.trim();
    if (key == "ssid") {
      inEntry = val.length() > 0 && knownCount < maxKnownNets;
      if (inEntry) knownSSID[knownCount++] = val;
    } else if (key == "pass" && inEntry) knownPass[knownCount - 1] = val;
    else if (key == "wigle_auth") wigleAuth = val;
    else if (key == "donate") wigleDonate = (val == "on" || val == "true");
  }
  f.close();
  if (wigleAuth.length() == 0) knownCount = 0;  // nothing to upload with
  Serial.println("Auto-upload: " + String(knownCount) + " known network(s).");
}

int findKnownNetwork(int numNetworks) {
  for (int i = 0; i < numNetworks; i++)
    for (int k = 0; k < knownCount; k++)
      if (WiFi.SSID(i) == knownSSID[k]) return k;
  return -1;
}

// Call right after a scan, while its results are still valid.
void maybeUpload(int numNetworks) {
  int k = findKnownNetwork(numNetworks);
  if (k < 0) {
    uploadDone = false;  // left the known area: upload again on the next visit
    return;
  }
  if (uploadDone || (lastUploadAttempt != 0 && millis() - lastUploadAttempt < uploadRetryMs)) return;
  lastUploadAttempt = millis();
  uploadDone = uploadPending(k);
}

// Uploads every /wifi-scans-*.csv with data rows and moves it to /uploaded/. True if all succeeded.
bool uploadPending(int k) {
  Serial.println("Known network " + knownSSID[k] + ", connecting...");
  M5.dis.drawpix(0, 0x0000ff);  // blue while uploading
  WiFi.begin(knownSSID[k].c_str(), knownPass[k].c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(250);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connect failed.");
    WiFi.disconnect();
    M5.dis.clear();
    return false;
  }

  // Collect names first; renaming while iterating the directory is unsafe.
  String names[50];
  int count = 0;
  File root = SD.open("/");
  for (File f = root.openNextFile(); f && count < 50; f = root.openNextFile()) {
    String name = f.name();
    if (!f.isDirectory() && name.startsWith("wifi-scans-") && name.endsWith(".csv") && hasDataRows(f))
      names[count++] = name;
    f.close();
  }
  root.close();
  SD.mkdir("/uploaded");

  bool allOk = true;
  for (int i = 0; i < count; i++) {
    if (uploadFile("/" + names[i])) {
      SD.rename("/" + names[i], "/uploaded/" + names[i]);
      Serial.println("Uploaded " + names[i]);
    } else {
      allOk = false;
      Serial.println("Upload failed: " + names[i]);
    }
  }
  WiFi.disconnect();
  M5.dis.clear();
  return allOk;
}

// The first two lines are the Wigle headers; anything after them is scan data.
bool hasDataRows(File& f) {
  int newlines = 0;
  while (f.available() && newlines < 3)
    if (f.read() == '\n') newlines++;
  return newlines >= 3;
}

bool uploadFile(const String& path) {
  File f = SD.open(path);
  if (!f) return false;
  WiFiClientSecure client;
  client.setCACert(ISRG_ROOT_X1);
  client.setTimeout(20000);
  if (!client.connect("api.wigle.net", 443)) {
    f.close();
    return false;
  }

  const String boundary = "----AtomGPSwigler";
  String head = "";
  if (wigleDonate)
    head += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"donate\"\r\n\r\non\r\n";
  head += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"" + path.substring(1) +
          "\"\r\nContent-Type: text/csv\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  client.print("POST /api/v2/file/upload HTTP/1.1\r\n"
               "Host: api.wigle.net\r\n"
               "Authorization: Basic " + wigleAuth + "\r\n"
               "User-Agent: AtomGPS-wigler\r\n"
               "Accept: application/json\r\n"
               "Connection: close\r\n"
               "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
               "Content-Length: " + String(head.length() + f.size() + tail.length()) + "\r\n\r\n");
  client.print(head);
  uint8_t buf[1024];
  size_t n;
  while ((n = f.read(buf, sizeof(buf))) > 0) client.write(buf, n);
  f.close();
  client.print(tail);

  String status = client.readStringUntil('\n');
  String body = client.readString();
  client.stop();
  bool ok = status.indexOf(" 200 ") > 0 && body.indexOf("\"success\":true") >= 0;
  if (!ok) Serial.println("WiGLE: " + status + " " + body.substring(0, 200));
  return ok;
}

const char* getAuthType(uint8_t wifiAuth) {
  switch (wifiAuth) {
    case WIFI_AUTH_OPEN:
      return "[OPEN]";
    case WIFI_AUTH_WEP:
      return "[WEP]";
    case WIFI_AUTH_WPA_PSK:
      return "[WPA_PSK]";
    case WIFI_AUTH_WPA2_PSK:
      return "[WPA2_PSK]";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "[WPA_WPA2_PSK]";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "[WPA2_ENTERPRISE]";
    case WIFI_AUTH_WPA3_PSK:
      return "[WPA3_PSK]";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "[WPA2_WPA3_PSK]";
    case WIFI_AUTH_WAPI_PSK:
      return "[WAPI_PSK]";
    default:
      return "[UNDEFINED]";
  }
}
