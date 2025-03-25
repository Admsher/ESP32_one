#include "SD.h"
#include "AudioTools.h"
#include "AudioTools/AudioLibs/A2DPStream.h"
#include "AudioTools/Disk/AudioSourceSDFAT.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include <base64.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include "esp_camera.h"
#include "FS.h"
#include "SPI.h"
#include <fstream>
#include <ArduinoJson.h>


#define CAMERA_MODEL_ESP_EYE 
#include "camera_pins.h"

const char *ssid = "r";
const char *password = "";
URLStream url(ssid, password); // Music Stream
StreamCopy copier; //(i2s, music, 1024); // copy music to i2s
File file; // final output stream

const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSDFAT source(startFilePath, ext,15); // , PIN_AUDIO_KIT_SD_CARD_CS);
A2DPStream out;
MP3DecoderHelix decoder;
AudioPlayer player(source, out, decoder);


const char* name = "LE-Flawless";        
AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(32000);               // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in(sineWave);               // Stream generated from sine wave
                                    
StreamCopy copymusic(out, in); // copy in to out



unsigned long lastCaptureTime = 0; // Last shooting time
int imageCount = 1;                // File Counter
bool camera_sign = false;          // Check camera status
bool sd_sign = false;              // Check sd status


// Name of the server we want to connect to
const char kHostname[] = "";
const int kPort = 443;

// Path to download (this is the bit after the hostname in the URL
// that you want to download
const char kPath[] = "/upload1";
const char kPath1[] = "/esp32";

int pictureNumber = 0;

uint8_t *fb_buf;
size_t fb_len;



void send_photo() {
    WiFiClient client;
    
    // Capture photo from camera
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return;
    }

    // Encode image to Base64
    String encoded = base64::encode(fb->buf, fb->len);
    
    esp_camera_fb_return(fb);  // Release frame buffer

    // Send encoded image over WiFi
    if (client.connect(kHostname,kPort)) {
        Serial.println("Connected to server, sending data...");

        client.println("POST /upload1 HTTP/1.1");
        client.println("Host: ");
        client.println("Content-Type: application/x-www-form-urlencoded");
        client.println("Content-Length: " + String(encoded.length()));
        client.println();
        client.println(encoded);
        client.println();
        
        Serial.println("Photo sent successfully");
    } else {
        Serial.println("Connection to server failed");
    }

    client.stop();  // Close connection
}


void receivedata(void) {
    // intialize SD
    if(!SD.begin()){   
        LOGE("SD failed");
        return;
    }
    Serial.println("Fetching..");
    // open music stream
    url.begin("https://:443/esp32/audio.mp3");
    Serial.println("Received");
    // copy file
    file = SD.open("audio.mp3", FILE_WRITE);
    file.seek(0); // overwrite from beginning
    copier.begin(file, url);
    copier.copyAll();
    // file.close();
   
}


void setup(){
    initArduino();  // Required to initialize Arduino framework
    Serial.begin(115200);

    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);
 

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.frame_size = FRAMESIZE_UXGA;
    config.pixel_format = PIXFORMAT_JPEG;  // for streaming
    //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
    //                      for larger pre-allocated frame buffer.
    if (config.pixel_format == PIXFORMAT_JPEG) {
        if (psramFound()) {
        config.jpeg_quality = 10;
        config.fb_count = 2;
        config.grab_mode = CAMERA_GRAB_LATEST;
        } else {
        // Limit the frame size when PSRAM is not available
        config.frame_size = FRAMESIZE_SVGA;
        config.fb_location = CAMERA_FB_IN_DRAM;
        }
    } 

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
    
    camera_sign = true; // Camera initialization check passes



    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi..");
        Serial.println(WiFi.status());

    }
    Serial.println("Connected to WiFi!");


    SPI.begin(14,12,13,15);

    if(!SD.begin(15)){
        Serial.println("Card Mount Failed");
        return;
        }
        uint8_t cardType = SD.cardType();

        // Determine if the type of SD card is available
        if(cardType == CARD_NONE){
        Serial.println("No SD card attached");
        return;
        }

        Serial.print("SD Card Type: ");
        if(cardType == CARD_MMC){
        Serial.println("MMC");
        } else if(cardType == CARD_SD){
        Serial.println("SDSC");
        } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
        } else {
        Serial.println("UNKNOWN");
        }


    sd_sign = true; // sd initialization check passes
    



    // setup player
    // Setting up SPI if necessary with the right SD pins by calling 
    SPI.begin(14,12,13,15);
    player.setVolume(0.8);
    player.begin();

    // setup output - We send the test signal via A2DP - so we conect to a Bluetooth Speaker
    auto cfg = out.defaultConfig(TX_MODE);
    cfg.silence_on_nodata = true; // prevent disconnect when there is no audio data
    cfg.name = "LE-FLawless";  // set the device here. Otherwise the first available device is used for output
    //cfg.auto_reconnect = true;  // if this is use we just quickly connect to the last device ignoring cfg.name
    out.begin(cfg);





}


void loop(){
    // take a photo every 24 seconds
    unsigned long now = millis();
    if ((now - lastCaptureTime) >= 24000) {
        char filename[32];
        send_photo();
        receivedata();
        // player.copy();
        
        Serial.println("Photos will begin in one 24 seconds, please be ready.");
        imageCount++;
        lastCaptureTime = now;
    }
}




