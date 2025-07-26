#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h> // https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_ST7789.h> // https://github.com/adafruit/Adafruit-ST7735-Library
#include <Adafruit_MLX90640.h> // https://github.com/adafruit/Adafruit_MLX90640

// ==================== PIN DEFINITIONS ====================
// Display pins (SPI)
#define TFT_CS    13
#define TFT_RST   15
#define TFT_DC    14
#define TFT_MOSI  16
#define TFT_SCLK  12

// I2C pins for MLX90640
#define SDA_PIN   21
#define SCL_PIN   18

// Control pins
#define BUTTON_1_PIN 7 // CYCLE
#define BUTTON_2_PIN 6 // PWR
#define BUTTON_3_PIN 5 // OK

// ==================== DISPLAY SETUP ====================
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// Display dimensions
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define THERMAL_WIDTH 240
#define THERMAL_HEIGHT 180

// ==================== MLX90640 SETUP ====================
Adafruit_MLX90640 mlx;
float frame[32*24];

// ==================== COLOR PALETTES ====================
enum ColorPalette {
  PALETTE_IRON = 0,
  PALETTE_RAINBOW,
  PALETTE_GRAYSCALE,
  PALETTE_HOT,
  PALETTE_JET,
  PALETTE_COUNT
};

ColorPalette currentPalette = PALETTE_IRON;
const char* paletteNames[] = {"Iron", "Rainbow", "Grayscale", "Hot", "Jet"};

// Temperature range for color mapping
float minTemp = 20.0;
float maxTemp = 40.0;

// Hot/cold spot tracking
float hottest = 20.0;
float coldest = 40.0;
int hottestIdx = 0;
int coldestIdx = 0;

// Frame rate control
unsigned long lastFrameTime = 0;
const unsigned long frameInterval = 62; // ~16 FPS (62ms)

// GUI update control
unsigned long lastGUIUpdate = 0;
const unsigned long guiInterval = 200; // 5 FPS (200ms)

// Button debounce
unsigned long lastButtonPress = 0;
bool lastButtonState = HIGH;
const unsigned long debounceDelay = 300;

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("IRview Starting...");
  
  // Initialize button
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  
  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  
  // Initialize the display
  tft.init(SCREEN_WIDTH, SCREEN_HEIGHT, SPI_MODE0);
  tft.setRotation(0); // Portrait mode
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  
  // Show startup message
  tft.setCursor(20, 50);
  tft.println("IRview");
  tft.setTextSize(1);
  tft.setCursor(20, 80);
  tft.println("Initializing MLX90640...");
  
  // Initialize MLX90640
  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("MLX90640 not found!");
    tft.setCursor(20, 100);
    tft.setTextColor(ST77XX_RED);
    tft.println("MLX90640 ERROR!");
    while (1) delay(10);
  }
  Serial.println("Found Adafruit MLX90640");
  
  // Set refresh rate
  mlx.setRefreshRate(MLX90640_16_HZ);
  Serial.print("Refresh rate set to: ");
  mlx90640_refreshrate_t rate = mlx.getRefreshRate();
  switch(rate) {
    case MLX90640_0_5_HZ: Serial.println("0.5 Hz"); break;
    case MLX90640_1_HZ: Serial.println("1 Hz"); break;
    case MLX90640_2_HZ: Serial.println("2 Hz"); break;
    case MLX90640_4_HZ: Serial.println("4 Hz"); break;
    case MLX90640_8_HZ: Serial.println("8 Hz"); break;
    case MLX90640_16_HZ: Serial.println("16 Hz"); break;
    case MLX90640_32_HZ: Serial.println("32 Hz"); break;
    case MLX90640_64_HZ: Serial.println("64 Hz"); break;
  }
  
  // Set mode
  mlx.setMode(MLX90640_CHESS);
  
  Serial.println("MLX90640 initialized successfully");
  
  // Clear screen and show ready message
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(20, 50);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.println("Ready!");
  tft.setTextSize(1);
  tft.setCursor(20, 80);
  tft.setTextColor(ST77XX_CYAN);
  tft.println("Press CYCLE to change palette");
  delay(2000);
}

// ==================== MAIN LOOP ====================
void loop() {
  unsigned long currentTime = millis();
  
  // Handle button input
  handleButton();
  
  // Read thermal data at specified frame rate
  if (currentTime - lastFrameTime >= frameInterval) {
    if (readThermalData()) {
      // Process thermal data in one pass
      processFrame();
      
      // Render interpolated thermal image
      renderThermalImage();
      
      // Update GUI less frequently
      if (currentTime - lastGUIUpdate >= guiInterval) {
        drawUI();
        lastGUIUpdate = currentTime;
      }
      
      lastFrameTime = currentTime;
    }
  }
  
  delay(10);
}

// ==================== INPUT HANDLING ====================
void handleButton() {
  bool buttonState = digitalRead(BUTTON_1_PIN);
  unsigned long currentTime = millis();
  
  if (buttonState == LOW && lastButtonState == HIGH && 
      currentTime - lastButtonPress > debounceDelay) {
    currentPalette = (ColorPalette)((currentPalette + 1) % PALETTE_COUNT);
    lastButtonPress = currentTime;
    Serial.print("Switched to palette: ");
    Serial.println(paletteNames[currentPalette]);
    
    // Show palette change on screen briefly (force immediate GUI update)
    drawUI();
    lastGUIUpdate = currentTime;
  }
  lastButtonState = buttonState;
}

// ==================== MLX90640 FUNCTIONS ====================
bool readThermalData() {
  if (mlx.getFrame(frame) != 0) {
    Serial.println("Failed to read frame data");
    return false;
  }
  return true;
}

void updateTemperatureRange() {
  float tempMin = frame[0];
  float tempMax = frame[0];
  
  // Find min/max temperatures in the frame
  for (int i = 1; i < 768; i++) {
    if (frame[i] < tempMin) tempMin = frame[i];
    if (frame[i] > tempMax) tempMax = frame[i];
  }
  
  // Smooth the range changes to avoid flickering
  minTemp = minTemp * 0.9 + tempMin * 0.1;
  maxTemp = maxTemp * 0.9 + tempMax * 0.1;
  
  // Ensure minimum range for better visualization
  if (maxTemp - minTemp < 2.0) {
    float center = (maxTemp + minTemp) / 2.0;
    minTemp = center - 1.0;
    maxTemp = center + 1.0;
  }
}

// ==================== RENDERING FUNCTIONS ====================
void renderThermalImage() {
  const float scaleX = (float)THERMAL_WIDTH / 32.0;   // 7.5 pixels per thermal pixel
  const float scaleY = (float)THERMAL_HEIGHT / 24.0;  // 7.5 pixels per thermal pixel
  const int offsetY = 40; // Offset for UI space at top
  
  // Render each thermal pixel as a block
  // MLX90640 data: 32 columns × 24 rows
  for (int row = 0; row < 24; row++) {
    for (int col = 0; col < 32; col++) {
      float temperature = frame[row * 32 + col]; // Row-major indexing
      uint16_t color = temperatureToColor(temperature);
      
      // Calculate pixel block dimensions
      int startX = col * scaleX;
      int startY = row * scaleY + offsetY;
      int blockWidth = ((col + 1) * scaleX) - startX;
      int blockHeight = ((row + 1) * scaleY) - startY;
      
      // Fill the pixel block
      tft.fillRect(startX, startY, blockWidth, blockHeight, color);
    }
  }
}

// ==================== MOSTLY "BORROWED" CODE ====================

// Bilinear interpolation function
float bilinearInterpolate(float x, float y) {
  x = constrain(x, 0, 31.999);
  y = constrain(y, 0, 23.999);

  int x1 = (int)x;
  int y1 = (int)y;
  int x2 = min(x1 + 1, 31);
  int y2 = min(y1 + 1, 23);

  float fx = x - x1;
  float fy = y - y1;

  float temp11 = frame[y1 * 32 + x1];
  float temp21 = frame[y1 * 32 + x2];
  float temp12 = frame[y2 * 32 + x1];
  float temp22 = frame[y2 * 32 + x2];

  float temp1 = temp11 * (1 - fx) + temp21 * fx;
  float temp2 = temp12 * (1 - fx) + temp22 * fx;

  return temp1 * (1 - fy) + temp2 * fy;
}

// Render smooth interpolated thermal image
void renderThermalImage() {
  const int offsetY = 40; // Offset for UI space at top
  
  // Render pixel by pixel with interpolation
  for (int screenY = 0; screenY < THERMAL_HEIGHT; screenY++) {
    for (int screenX = 0; screenX < THERMAL_WIDTH; screenX++) {
      float thermalX = (float)screenX * 31.0 / (THERMAL_WIDTH - 1);
      float thermalY = (float)screenY * 23.0 / (THERMAL_HEIGHT - 1);
      
      float temperature = bilinearInterpolate(thermalX, thermalY);
      uint16_t color = temperatureToColor(temperature);
      
      tft.drawPixel(screenX, screenY + offsetY, color);
    }
  }
}

uint16_t temperatureToColor(float temp) {
  float normalized = (temp - minTemp) / (maxTemp - minTemp);
  normalized = constrain(normalized, 0.0, 1.0);
  
  uint8_t r, g, b;
  
  switch (currentPalette) {
    case PALETTE_IRON:
      getIronColor(normalized, &r, &g, &b);
      break;
    case PALETTE_RAINBOW:
      getRainbowColor(normalized, &r, &g, &b);
      break;
    case PALETTE_GRAYSCALE:
      r = g = b = normalized * 255;
      break;
    case PALETTE_HOT:
      getHotColor(normalized, &r, &g, &b);
      break;
    case PALETTE_JET:
      getJetColor(normalized, &r, &g, &b);
      break;
  }
  
  return tft.color565(r, g, b);
}

// ==================== COLOR PALETTE FUNCTIONS ====================
void getIronColor(float value, uint8_t* r, uint8_t* g, uint8_t* b) {
  if (value < 0.25) {
    *r = 0;
    *g = 0;
    *b = value * 4 * 255;
  } else if (value < 0.5) {
    *r = 0;
    *g = (value - 0.25) * 4 * 255;
    *b = 255;
  } else if (value < 0.75) {
    *r = (value - 0.5) * 4 * 255;
    *g = 255;
    *b = 255 - (value - 0.5) * 4 * 255;
  } else {
    *r = 255;
    *g = 255;
    *b = 255 - (value - 0.75) * 4 * 255;
  }
}

void getRainbowColor(float value, uint8_t* r, uint8_t* g, uint8_t* b) {
  float hue = value * 300; // 0 to 300 degrees (blue to red)
  hsvToRgb(hue, 1.0, 1.0, r, g, b);
}

void getHotColor(float value, uint8_t* r, uint8_t* g, uint8_t* b) {
  if (value < 0.33) {
    *r = value * 3 * 255;
    *g = 0;
    *b = 0;
  } else if (value < 0.66) {
    *r = 255;
    *g = (value - 0.33) * 3 * 255;
    *b = 0;
  } else {
    *r = 255;
    *g = 255;
    *b = (value - 0.66) * 3 * 255;
  }
}

void getJetColor(float value, uint8_t* r, uint8_t* g, uint8_t* b) {
  if (value < 0.125) {
    *r = 0;
    *g = 0;
    *b = 128 + value * 8 * 127;
  } else if (value < 0.375) {
    *r = 0;
    *g = (value - 0.125) * 4 * 255;
    *b = 255;
  } else if (value < 0.625) {
    *r = (value - 0.375) * 4 * 255;
    *g = 255;
    *b = 255 - (value - 0.375) * 4 * 255;
  } else if (value < 0.875) {
    *r = 255;
    *g = 255 - (value - 0.625) * 4 * 255;
    *b = 0;
  } else {
    *r = 255 - (value - 0.875) * 8 * 127;
    *g = 0;
    *b = 0;
  }
}

void hsvToRgb(float h, float s, float v, uint8_t* r, uint8_t* g, uint8_t* b) {
  int i = int(h / 60) % 6;
  float f = (h / 60) - i;
  float p = v * (1 - s);
  float q = v * (1 - s * f);
  float t = v * (1 - s * (1 - f));
  
  switch (i) {
    case 0: *r = v * 255; *g = t * 255; *b = p * 255; break;
    case 1: *r = q * 255; *g = v * 255; *b = p * 255; break;
    case 2: *r = p * 255; *g = v * 255; *b = t * 255; break;
    case 3: *r = p * 255; *g = q * 255; *b = v * 255; break;
    case 4: *r = t * 255; *g = p * 255; *b = v * 255; break;
    case 5: *r = v * 255; *g = p * 255; *b = q * 255; break;
  }
}

// ==================== END MOSTLY "BORROWED" CODE ====================

// ==================== UI FUNCTIONS ====================
void drawUI() {
  // Clear top area for UI
  tft.fillRect(0, 0, SCREEN_WIDTH, 40, ST77XX_BLACK);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.print("Min: ");
  tft.print(minTemp, 1);
  tft.print("C  Max: ");
  tft.print(maxTemp, 1);
  tft.print("C");

  tft.setCursor(5, 18);
  tft.print("Palette: ");
  tft.print(paletteNames[currentPalette]);
  
  // Clear bottom area for additional info
  const int bottomY = THERMAL_HEIGHT + 40;
  tft.fillRect(0, bottomY, SCREEN_WIDTH, SCREEN_HEIGHT - bottomY, ST77XX_BLACK);

  int centerIndex =  367; // Row 11, Column 15 (center-ish of 32×24 grid)
  float centerTemp = frame[centerIndex];
  
  // Display center temperature
  tft.setTextSize(2);
  tft.setCursor(10, bottomY + 10);
  tft.setTextColor(ST77XX_YELLOW);
  tft.print("Center: ");
  tft.print(centerTemp, 1);
  tft.print("C");
  
  // Display hottest spot
  tft.setTextSize(1);
  tft.setCursor(10, bottomY + 35);
  tft.setTextColor(ST77XX_RED);
  tft.print("Hottest: ");
  tft.print(hottest, 1);
  tft.print("C");
  
  // Display coldest spot
  tft.setCursor(10, bottomY + 50);
  tft.setTextColor(ST77XX_CYAN);
  tft.print("Coldest: ");
  tft.print(coldest, 1);
  tft.print("C");
  
  // Draw crosshair on the thermal image
  int crossX = THERMAL_WIDTH / 2;
  int crossY = THERMAL_HEIGHT / 2 + 40;
  tft.drawFastHLine(crossX - 8, crossY, 16, ST77XX_WHITE);
  tft.drawFastVLine(crossX, crossY - 8, 16, ST77XX_WHITE);
  
  // Draw hottest spot marker (small circle)
  int hotCol = hottestIdx % 32;
  int hotRow = hottestIdx / 32;
  int hotX = hotCol * (THERMAL_WIDTH / 32.0) + (THERMAL_WIDTH / 64.0);
  int hotY = hotRow * (THERMAL_HEIGHT / 24.0) + (THERMAL_HEIGHT / 48.0) + 40;
  tft.drawCircle(hotX, hotY, 3, ST77XX_RED);
  
  // Draw coldest spot marker (small square)
  int coldCol = coldestIdx % 32;
  int coldRow = coldestIdx / 32;
  int coldX = coldCol * (THERMAL_WIDTH / 32.0) + (THERMAL_WIDTH / 64.0);
  int coldY = coldRow * (THERMAL_HEIGHT / 24.0) + (THERMAL_HEIGHT / 48.0) + 40;
  tft.drawRect(coldX - 2, coldY - 2, 4, 4, ST77XX_CYAN);
}