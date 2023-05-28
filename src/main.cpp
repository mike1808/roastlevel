#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#include <MAX30105.h>
#include <Wire.h>

Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(128, 32, &Wire, -1);

MAX30105 particleSensor;

typedef struct readings {
  uint32_t red;
  uint32_t ir;
  uint32_t green;
  boolean read;
} reading;


readings unblocked;

#define PRESENSE_THRESHOLD 500

#define PRINT_READINGS

typedef struct roastlevel {
  uint32_t ir;
  const char *name;
} roastlevel;

#define endofroastlevel \
  roastlevel { \
    0 \
  }

// clang-format off
roastlevel roastLevels[] = {
    roastlevel{17370, "VERY DARK"}, 
    roastlevel{17720, "VERY DARK"},
    roastlevel{18070, "VERY DARK"}, 
    roastlevel{18420, "VERY DARK"},
    roastlevel{18770, "VERY DARK"}, 
    roastlevel{19120, "DARK"},
    roastlevel{19470, "DARK"},      
    roastlevel{19820, "DARK"},
    roastlevel{20170, "DARK"},      
    roastlevel{20520, "DARK"},
    roastlevel{20870, "MED. DARK"}, 
    roastlevel{21220, "MED. DARK"},
    roastlevel{21570, "MED. DARK"}, 
    roastlevel{21920, "MED. DARK"},
    roastlevel{22270, "MED. DARK"}, 
    roastlevel{22620, "MEDIUM"},
    roastlevel{22970, "MEDIUM"},    
    roastlevel{23320, "MEDIUM"},
    roastlevel{23670, "MEDIUM"},    
    roastlevel{24020, "MEDIUM"},
    roastlevel{24370, "MED. LIGHT"},
    roastlevel{24720, "MED. LIGHT"},
    roastlevel{25070, "MED. LIGHT"},
    roastlevel{25420, "MED. LIGHT"},
    roastlevel{25770, "MED. LIGHT"},
    roastlevel{26120, "LIGHT"},
    roastlevel{26470, "LIGHT"},     
    roastlevel{26820, "LIGHT"},
    roastlevel{27170, "LIGHT"},     
    roastlevel{27520, "LIGHT"},
    roastlevel{27870, "VERY LIGHT"},
    roastlevel{28220, "VERY LIGHT"},
    roastlevel{28570, "VERY LIGHT"},
    roastlevel{28920, "VERY LIGHT"},
    roastlevel{29270, "PRADYBEANS"},
    endofroastlevel,
};
// clang-format on

void printReading(reading *r, bool nl) {
  Serial.print(r->red);
  Serial.print(",");
  Serial.print(r->ir);
  Serial.print(",");
  Serial.print(r->green);
  if (nl) {
    Serial.println();
  } else {
    Serial.print(",");
  }
}

void pixelShowReading(reading *r, int level) {
  uint32_t c = byte(map(r->ir, 0, 35000, 0, (1 << 8) - 1)) << 16 | byte(map(level, 0, 30, 1, 1 << 8 - 1));

  // uint32_t c = uint32_t(byte(map(r->ir, 0, 1 << 14, 0, 1 << 8 - 1))) << 16 |
  //              uint32_t(byte(map(r->green, 0, 1 << 14, 0, 1 << 8 - 1))) << 8
  //              | uint32_t(byte(map(r->red, 0, 1 << 14, 0, 1 << 8 - 1)));

  pixels.fill(c);
  pixels.show();
}
void displayReadings(reading *r) {
  display.setTextSize(2);  // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("IR "));
  display.print(r->ir);
  display.display();  // Show initial text
}

void displayLevel(roastlevel *l) {
  display.setTextSize(2);  // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 16);
  display.print(l->name);
  display.display();
}



readings read() {
  readings value;

  if (!particleSensor.available()) {
    value.read = false;
    return value;
  }

  value.read = true;
  value.red = particleSensor.getRed();
  value.ir = particleSensor.getIR();
  value.green = particleSensor.getGreen();
  particleSensor.nextSample();

#ifdef PRINT_READINGS
  printReading(&value, false);
  display.clearDisplay();
  displayReadings(&value);
#endif

  return value;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  display.display();

#if defined(NEOPIXEL_POWER)
  // If this board has a power control pin, we must set it to output and high
  // in order to enable the NeoPixels. We put this in an #if defined so it can
  // be reused for other boards without compilation errors
  pinMode(NEOPIXEL_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_POWER, HIGH);
#endif

  pixels.begin();             // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setBrightness(255);  // not so bright

  pixels.fill(0x0000FF);
  pixels.show();

  // Initialize sensor
  if (!particleSensor.begin(
        Wire1, I2C_SPEED_FAST))  // Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    pixels.fill(0xFF0000);
    pixels.show();
    while (1)
      ;
  }

  // Let's configure the sensor to run fast so we can over-run the buffer and
  // cause an interrupt
  byte ledBrightness = 0x0a;  // Options: 0=Off to 255=50mA
  byte sampleAverage = 8;     // Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2;           // Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  int sampleRate = 400;       // Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411;       // Options: 69, 118, 215, 411
  int adcRange = 2048;       // Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate,
                       pulseWidth,
                       adcRange);  // Configure sensor with these settings

  // turn off red
  particleSensor.setPulseAmplitudeRed(0); 
  particleSensor.enableAFULL();  // Enable the almost full interrupt (default is
                                 // 32 samples)

  particleSensor.setFIFOAlmostFull(
    3);  // Set almost full int to fire at 29 samples

  Serial.println("Averaging unblocked state values");
  // take the average for unblocked state

  int samples = 0;
#define INIT_SAMPLES (32)
#define INIT_ALPHA 0.98

  double ir = 0;

  while (samples < INIT_SAMPLES) {
    particleSensor.check();
    while (true) {
      readings value = read();

      if (!value.read)
        break;
      
      Serial.println();

      if (ir == 0) {
        ir = double(value.ir);
      } else {
        ir = INIT_ALPHA * ir + (1 - INIT_ALPHA) * double(value.ir);
      }
      samples++;
    }
  }

  unblocked.ir = uint32_t(ir);

  Serial.println("Unblocked values: ");
  printReading(&unblocked, true);

  Serial.println("Starting...");
}

void printLevel(int i) {
  Serial.print(i * 1000);
  Serial.print(",");
  Serial.print(roastLevels[i].name);
}

void detect(readings &r) {
  int i = 0;

  if ((r.ir - unblocked.ir) < roastLevels[0].ir) {
    pixelShowReading(&r, 0);
    printLevel(0);
    return;
  }

  roastlevel level;

  while (level = roastLevels[i++], level.ir != 0) {
    if (abs(int64_t(r.ir) - int64_t(unblocked.ir) - int64_t(level.ir))
        <= 1000) {
      break;
    }
  }

  printLevel(i - 1);
  displayLevel(&roastLevels[i - 1]);
  pixelShowReading(&r, i - 1);
}

void loop() {
  particleSensor.check();  // Check the sensor, read up to 3 samples

  while (true) {
    readings value = read();
    if (!value.read) {
      break;
    }

    if (abs(int64_t(value.ir) - int64_t(unblocked.ir)) < PRESENSE_THRESHOLD) {
      Serial.println();
      continue;
    }

    detect(value);
    Serial.println();
  }
}

