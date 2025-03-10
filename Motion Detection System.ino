'']]kk hh #include <Arduino.h>
#include "esp_camera.h" // camera
#include "SD_MMC.h" // SD card      
#include <EEPROM.h> // EEPROM     
#include "esp_sleep.h" // Sleep functions

// define the number of bytes you want to access
#define EEPROM_SIZE 1
unsigned char pictureNumber = 0; // 0..255

gpio_num_t pirPin = GPIO_NUM_13; // signal pin of the PIR sensor

// ESP32Cam (AiThinker) PIN Map
static camera_config_t camera_config = {
  .pin_pwdn = 32,
  .pin_reset = -1,
  .pin_xclk = 0,
  .pin_sscb_sda = 26,
  .pin_sscb_scl = 27,

  .pin_d7 = 35,
  .pin_d6 = 34,
  .pin_d5 = 39,
  .pin_d4 = 36,
  .pin_d3 = 21,
  .pin_d2 = 19,
  .pin_d1 = 18,
  .pin_d0 = 5,
  .pin_vsync = 25,
  .pin_href = 23,
  .pin_pclk = 22,

  //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
  .xclk_freq_hz = 20000000,
  .ledc_timer = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,

  .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG

  .frame_size = FRAMESIZE_UXGA, //FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA (Do not use sizes above QVGA when not JPEG)
  .jpeg_quality = 6, //0-63 lower number means higher quality
  .fb_count = 1      //if more than one, i2s runs in continuous mode. Use only with JPEG
};

static esp_err_t init_camera()
{
  pinMode(pirPin, INPUT);     // declare PIR sensor as input

  // Initialize the camera
  Serial.println("Initializing camera");
  delay(500);
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return err;
  }

  return ESP_OK;
}

static esp_err_t init_sd_card()
{
  // Start SD Card
  Serial.println("Starting SD Card");
  delay(500);
  if (!SD_MMC.begin())
  {
    Serial.println("SD Card Mount Failed");
    return ESP_ERR_NO_MEM;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD Card attached");
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}

static String generate_file_name()
{
  // initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);
  pictureNumber = EEPROM.read(0) + 1;

  // Path where new picture will be saved in SD Card
  return "/picture" + String(pictureNumber) + ".jpg";
}

static esp_err_t save_pic (camera_fb_t * fb, String path)
{
  fs::FS &fs = SD_MMC;
  Serial.printf("Picture file name: %s\n", path.c_str());

  File file = fs.open(path.c_str(), FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file in writing mode");
    esp_camera_fb_return(fb);
    return ESP_ERR_NO_MEM;
  }
  else
  {
    file.write(fb->buf, fb->len); // payload (image), payload length
    EEPROM.write(0, pictureNumber);
    EEPROM.commit();
    file.close();
    esp_camera_fb_return(fb);
  }

  return ESP_OK;
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // Check wake-up reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  // If woken up by PIR sensor, add a delay
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0)
  {
    Serial.println("Motion detected, waiting for a few seconds...");
  }

  if (init_camera() == ESP_OK && init_sd_card() == ESP_OK)
  {
    // Warm-up loop to discard first few frames
    for (int i = 0; i < 10; i++) {
      camera_fb_t * fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("Failed to capture frame");
        continue;
      }
      esp_camera_fb_return(fb);
    }

    // Here I take the picture I will use:
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("Camera capture failed");
      return;
    }

    // Generate file name
    String path = generate_file_name();

    // Save picture
    if (save_pic(fb, path) == ESP_OK)
    {
      Serial.printf("Saved file to path: %s\n", path.c_str());
    }
    else
    {
      Serial.printf("Failed to save file to path: %s\n", path.c_str());
    }

    // PIR sensor
    esp_sleep_enable_ext0_wakeup(pirPin, RISING);

    Serial.println("Going to sleep now");

    // Sleeping until external wakeup triggered (motion detector)
    esp_deep_sleep_start(); a
  }
  else
  {
    Serial.println("Init of camera/SD card failed.");
  }
}

void loop()
{
  // this will never be called
}
