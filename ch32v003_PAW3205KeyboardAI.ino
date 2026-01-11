// ============================================================================
// CH32V003 ST7789 Virtual Keyboard with PAW3205 Optical Mouse
// ============================================================================
// Description: Virtual keyboard interface using optical mouse for character
//              selection. Displays A-Z, 0-9, and symbols on 76x284 display.
//              Selected characters form a string displayed at bottom.
// 
// Features:
// - Mouse-controlled cursor for character selection
// - Grid-based virtual keyboard layout (19x3 grid)
// - Real-time string display and editing
// - Delete character functionality
// - Enter function (green text when arrow selected)
// - Serial output of selected string
//
// Memory Usage: ~90% of 16KB Flash on CH32V003
// Author: Based on original code with optimizations and comments
// ============================================================================

// ============================================================================
// HARDWARE PIN DEFINITIONS
// ============================================================================
// Display control pins
#define TFT_CS   PD2    // Chip Select (active LOW)
#define TFT_DC   PD3    // Data/Command (HIGH=data, LOW=command)
#define TFT_RST  PD4    // Reset (active LOW)

// Display SPI pins (hardware SPI1 on CH32V003)
#define TFT_SCK  PC5    // SPI Clock
#define TFT_MOSI PC6    // SPI Data Out

// PAW3205 Optical Mouse Sensor pins (bit-banged SPI)
#define MOUSE_SCLK  PC0    // Serial Clock
#define MOUSE_SDIO  PD0    // Serial Data I/O (bidirectional)
#define MOUSE_SW    PC1    // Mouse button switch (active LOW with pull-up)

// Serial in/out pins (hardware UART1 on CH32V003)
#define TX  PD5    // Serial Out
#define RX  PD6    // Serial In
// ============================================================================
// DISPLAY COLOR DEFINITIONS (RGB565 Format)
// ============================================================================
// Note: Color names reflect actual RGB565 values
#define BL 0x001F  // Blue (not Red - corrected from original)
#define GN 0x07E0  // Green
#define RD 0xF800  // Red (not Blue - corrected from original)
#define WH 0xFFFF  // White
#define BK 0x0000  // Black
#define CY 0xFFE0  // Cyan
#define YL 0xFE20  // Yellow
#define MG 0xF81F  // Magenta
#define SL 0x7BEF  // Gray

/* Additional color reference for future use:
RED: 0x001F      GREEN: 0x07E0     BLUE: 0xF800
YELLOW: 0x07FF   MAGENTA: 0xF81F   CYAN: 0xFFE0
ORANGE: 0x04FF   PINK: 0xFC1F      PURPLE: 0x8010
LIME: 0x05E0     TEAL: 0x87E0      NAVY: 0x8000
MAROON: 0x0010   DARK GRAY: 0x4208 GRAY: 0x8410
LIGHT GRAY: 0xC618 SILVER: 0xA514  BROWN: 0x0210
OLIVE: 0x0410    FOREST GREEN: 0x0280 GOLD: 0x051F */

// ============================================================================
// DISPLAY CONFIGURATION CONSTANTS
// ============================================================================
#define DISPLAY_WIDTH  76     // Physical display width in pixels
#define DISPLAY_HEIGHT 284    // Physical display height in pixels
#define TFT_X_OFFSET   82     // ST7789 X offset for 76x284 display
#define TFT_Y_OFFSET   18     // ST7789 Y offset for 76x284 display

// MADCTL (Memory Data Access Control) register bit definitions
#define ST77XX_MADCTL_MY  0x80   // Row Address Order (bottom to top)
#define ST77XX_MADCTL_MX  0x40   // Column Address Order (right to left)
#define ST77XX_MADCTL_MV  0x20   // Row/Column Exchange (landscape mode)
#define ST77XX_MADCTL_ML  0x10   // Vertical Refresh Order
#define ST77XX_MADCTL_RGB 0x08   // RGB vs BGR color order

// ============================================================================
// KEYBOARD LAYOUT CONSTANTS
// ============================================================================
#define KEY_COLS       19     // 19 columns in keyboard grid
#define KEY_ROWS        3     // 3 rows in keyboard grid
#define KEY_WIDTH      15     // Each key is 15 pixels wide
#define KEY_HEIGHT     19     // Each key is 19 pixels tall
#define KEY_AREA_Y     42     // Y-position where keyboard area ends (3*19=57)
#define DEL_KEY_X     256     // Delete key X position
#define DEL_KEY_Y      59     // Delete key Y position
#define DEL_KEY_W      26     // Delete key width
#define DEL_KEY_H      15     // Delete key height
#define MAX_STRING_LEN 16     // Maximum characters in output string

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
// Display state variables
int textCol = WH, textBgr = BK;    // Current text and background colors
int tftIdx = 0, tftIdy = 0;        // Text cursor position (pixels)
int xStart, yStart;                // Display offset coordinates
byte textSize = 1;                 // Text size multiplier (1-4)
byte rotate = 0;                   // Display rotation (0-3)

// Mouse and cursor variables
int msx = 0, msy = 0;              // Mouse position (continuous, pixels)
int mx = 0, my = 0;                // Grid-aligned cursor position (grid cells)
String text = "";                  // Output string being constructed
byte flag = 0;                     // General purpose flag variable

// ============================================================================
// 5x7 FONT TABLE (95 printable ASCII characters, 32-127)
// ============================================================================
// Each character is 5 bytes wide, 7 bits high (LSB = top row)
// Organized as 95 characters * 5 bytes = 475 bytes total
// Annotated with character ranges for easy reference
const uint8_t Chartable[] = 
{ 
  // Characters 0x20-0x23: Space, !, ", #
  0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x4F,0x00,0x00, 0x00,0x07,0x00,0x07,0x00, 0x14,0x7F,0x14,0x7F,0x14,
  
  // Characters 0x24-0x27: $, %, &, '
  0x24,0x2A,0x7F,0x2A,0x12, 0x23,0x13,0x08,0x64,0x62, 0x36,0x49,0x55,0x22,0x50, 0x00,0x05,0x03,0x00,0x00,
  
  // Characters 0x28-0x2B: (, ), *, +
  0x00,0x1C,0x22,0x41,0x00, 0x00,0x41,0x22,0x1C,0x00, 0x14,0x08,0x3E,0x08,0x14, 0x08,0x08,0x3E,0x08,0x08,
  
  // Characters 0x2C-0x2F: ,, -, ., /
  0x00,0x00,0x50,0x30,0x00, 0x10,0x10,0x10,0x10,0x10, 0x00,0x60,0x60,0x00,0x00, 0x20,0x10,0x08,0x04,0x02,
  
  // Characters 0x30-0x33: 0, 1, 2, 3
  0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00, 0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31,
  
  // Characters 0x34-0x37: 4, 5, 6, 7
  0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x30, 0x01,0x71,0x09,0x05,0x03,
  
  // Characters 0x38-0x3B: 8, 9, :, ;
  0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E, 0x00,0x36,0x36,0x00,0x00, 0x00,0x56,0x36,0x00,0x00,
  
  // Characters 0x3C-0x3F: <, =, >, ?
  0x08,0x14,0x22,0x41,0x00, 0x14,0x14,0x14,0x14,0x14, 0x00,0x41,0x22,0x14,0x08, 0x02,0x01,0x51,0x09,0x06,
  
  // Characters 0x40-0x43: @, A, B, C
  0x32,0x49,0x59,0x51,0x3E, 0x7E,0x11,0x11,0x11,0x7E, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
  
  // Characters 0x44-0x47: D, E, F, G
  0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01, 0x3E,0x41,0x49,0x49,0x7A,
  
  // Characters 0x48-0x4B: H, I, J, K
  0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00, 0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41,
  
  // Characters 0x4C-0x4F: L, M, N, O
  0x7F,0x40,0x40,0x40,0x40, 0x7F,0x02,0x0C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
  
  // Characters 0x50-0x53: P, Q, R, S
  0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46, 0x46,0x49,0x49,0x49,0x31,
  
  // Characters 0x54-0x57: T, U, V, W
  0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F, 0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F,
  
  // Characters 0x58-0x5B: X, Y, Z, [
  0x63,0x14,0x08,0x14,0x63, 0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43, 0x00,0x7F,0x41,0x41,0x00,
  
  // Characters 0x5C-0x5F: \, ], ^, _
  0x02,0x04,0x08,0x10,0x20, 0x00,0x41,0x41,0x7F,0x00, 0x11,0x39,0x55,0x11,0x1F, 0x40,0x40,0x40,0x40,0x40,
  
  // Characters 0x60-0x63: `, a, b, c
  0x10,0x38,0x54,0x10,0x1F, 0x20,0x54,0x54,0x54,0x78, 0x7F,0x48,0x44,0x44,0x38, 0x38,0x44,0x44,0x44,0x20,
  
  // Characters 0x64-0x67: d, e, f, g
  0x38,0x44,0x44,0x48,0x7F, 0x38,0x54,0x54,0x54,0x18, 0x08,0x7E,0x09,0x01,0x02, 0x0C,0x52,0x52,0x52,0x3E,
  
  // Characters 0x68-0x6B: h, i, j, k
  0x7F,0x08,0x04,0x04,0x78, 0x00,0x44,0x7D,0x40,0x00, 0x20,0x40,0x44,0x3D,0x00, 0x7F,0x10,0x28,0x44,0x00,
  
  // Characters 0x6C-0x6F: l, m, n, o
  0x00,0x41,0x7F,0x40,0x00, 0x7C,0x04,0x18,0x04,0x78, 0x7C,0x08,0x04,0x04,0x78, 0x38,0x44,0x44,0x44,0x38,
  
  // Characters 0x70-0x73: p, q, r, s
  0x7C,0x14,0x14,0x14,0x08, 0x08,0x14,0x14,0x18,0x7C, 0x7C,0x08,0x04,0x04,0x08, 0x48,0x54,0x54,0x54,0x20,
  
  // Characters 0x74-0x77: t, u, v, w
  0x04,0x3F,0x44,0x40,0x20, 0x3C,0x40,0x40,0x20,0x7C, 0x1C,0x20,0x40,0x20,0x1C, 0x3C,0x40,0x30,0x40,0x3C,
  
  // Characters 0x78-0x7B: x, y, z, {
  0x44,0x28,0x10,0x28,0x44, 0x0C,0x50,0x50,0x50,0x3C, 0x44,0x64,0x54,0x4C,0x44, 0x11,0x39,0x55,0x11,0x1F,
  
  // Characters 0x7C-0x7F: |, }, ~, DEL
  0x00,0x00,0x77,0x00,0x00, 0x20,0x54,0x57,0x54,0x78, 0x38,0x54,0x57,0x54,0x18, 0x00,0x48,0x7B,0x40,0x00,
  
  // Extended characters for custom shapes
  0x38,0x44,0x47,0x44,0x38, 0x38,0x47,0x44,0x47,0x38, 0x38,0x45,0x44,0x45,0x38, 0x3C,0x40,0x47,0x20,0x7C,
  0x3C,0x47,0x40,0x27,0x7C, 0x3C,0x41,0x40,0x21,0x7C
};

// ============================================================================
// KEYBOARD CHARACTER SET
// ============================================================================
// 57 characters arranged in 19x3 grid (A-Z, 0-9, symbols)
// Character 0x7B ({) is used as ENTER/SEND function (text turns green)
char keyboardChars[] = {
  'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S',
  'T','U','V','W','X','Y','Z','0','1','2','3','4','5','6','7','8','9',' ','{',
  '+','-','*','/','!','&','|','%','[',']','(',')','.',',',':',';','<','=','>'
};

// ============================================================================
// SETUP FUNCTION - Hardware Initialization
// ============================================================================
void setup() {
  // ========================================================================
  // SERIAL COMMUNICATION INITIALIZATION (Hardware UART1)
  // ========================================================================
  //Enable clock
   RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1;
  // Configure UART1 pins (PD5=TX, PD6=RX)
  GPIOD->CFGLR &= ~(0xF << (5*4));  // Clear PD5
  GPIOD->CFGLR &= ~(0xF << (6*4));  // Clear PD6
   // PD5 (TX) as alternate function push-pull 50MHz
  GPIOD->CFGLR |= (0xB << (5*4));  // 0xB = GPIO_Speed_50MHz + GPIO_CNF_OUT_PP_AF 
 // PD6 (RX) as input with pull-up/floating
  GPIOD->CFGLR |= (0x4 << (6*4));  // 0x4 = GPIO_CNF_IN_FLOAT
  // Configure USART1
  USART1->CTLR1 &= ~USART_CTLR1_UE;
  USART1->BRR = SystemCoreClock / 115200;
  USART1->CTLR1 = USART_WordLength_8b | USART_Parity_No | 
                  USART_Mode_Rx | USART_Mode_Tx;
  USART1->CTLR2 = USART_StopBits_1;
  USART1->CTLR3 = 0;
  USART1->CTLR1 |= USART_CTLR1_UE;
// Warm-up delay- needed for USB buffering   
 delay(2000);
  Serial_Println("CH32V003 Virtual Keyboard Active");
  // ========================================================================
  // MOUSE SENSOR INITIALIZATION
  // ========================================================================
  pinMode(MOUSE_SW, INPUT_PULLUP);    // Mouse button with internal pull-up
  pinMode(MOUSE_SCLK, OUTPUT);        // Mouse SPI clock
  pinMode(MOUSE_SDIO, OUTPUT);        // Mouse SPI data
  
  // Set idle states for mouse interface
  digitalWrite(MOUSE_SCLK, HIGH);     // Clock idle HIGH (Mode 0)
  digitalWrite(MOUSE_SDIO, HIGH);     // Data idle HIGH
  
  delay(100);  // Power stabilization delay
  
  // Initialize PAW3205 mouse sensor
  resync();                           // Synchronize serial interface
  writePAW(0x06, 0x80);               // Reset command
  delay(100);
  writePAW(0x06, 0x04);               // Configure motion wake-up (level detection)
  // Alternative: writePAW(0x06, 0x44); // Pulse detection mode
  
  // ========================================================================
  // DISPLAY HARDWARE INITIALIZATION
  // ========================================================================
  pinMode(TFT_CS, OUTPUT);            // Display chip select
  pinMode(TFT_DC, OUTPUT);            // Display data/command
  pinMode(TFT_RST, OUTPUT);           // Display reset
  
  // Initialize hardware SPI for display
  RCC->APB2PCENR |= RCC_APB2Periph_SPI1;  // Enable SPI1 peripheral clock
  
  // Configure SPI pins as alternate function
  GPIOC->CFGLR &= ~(0xF << (5*4) | 0xF << (6*4));  // Clear PC5, PC6 config
  GPIOC->CFGLR |= (0xB << (5*4)) | (0xB << (6*4)); // AF push-pull 50MHz
  
  // Configure SPI1 controller
  SPI1->CTLR1 = SPI_CTLR1_MSTR |    // Master mode
                SPI_CTLR1_SSM |     // Software slave management
                SPI_CTLR1_SSI;      // Internal slave select
  SPI1->CTLR1 |= SPI_CTLR1_SPE;     // Enable SPI peripheral
  
  // ========================================================================
  // DISPLAY SOFTWARE INITIALIZATION
  // ========================================================================
  InitTFT();                         // Initialize ST7789 controller
  setRotation(1);                    // Set landscape orientation (90° clockwise)
  fillScreen(BK);                    // Clear screen to black
  delay(100);
  
  // Draw initial interface
  graph();                           // Draw keyboard grid and interface
}

// ============================================================================
// MAIN LOOP - Application Logic
// ============================================================================
void loop() {
  // Check for mouse button press (active LOW)
  if (!(GPIOC->INDR & (1 << 1))) {
    addchar();  // Process character selection or deletion
  }
  
  // Check if mouse motion occurred (bit 7 of motion status register)
  if (readPAW(0x02) & 0x80) {
    mousPos();  // Update cursor position based on mouse movement
    graph();    // Refresh display (cursor highlight)
  } else {
    // No motion - handle periodic UI updates
    static byte counter;
    
    // Blink cursor every 200ms (5 times per second)
    if (millis() % 200 == 0) {
      counter++;
      
      // Toggle cursor highlight in keyboard area
      if (my < KEY_AREA_Y) {
        drawRect(mx, my, 14, 18, GN);
        drawRect(mx - 1, my - 1, 16, 20, GN);
      }
    }
    
    // Update display every second (when counter reaches 5)
    if (counter >= 5) {
      counter = 0;
      
      // Refresh the entire interface
      graph();
      
      // Redraw cursor in current position
      if (my < KEY_AREA_Y) {
        drawRect(mx, my, 14, 18, GN);
        drawRect(mx - 1, my - 1, 16, 20, GN);
      }
      
      // Draw interface labels
      setTextSize(1);
      tftCursor(262, 63);
      tftPrint("DEL");
      
      setTextSize(2);
      tftCursor(5, 60);
      tftPrint("Str=");
      
      // Draw all keyboard characters
      for (int i = 0; i < sizeof(keyboardChars); i++) {
        if (!(GPIOC->INDR & (1 << 1))) break;  // Exit if button pressed
        
        // Calculate character position (19 columns, 3 rows)
        tftCursor((i % 19) * 15 + 2, (i / 19) * 19 + 3);
        tftPrint(char(keyboardChars[i]));
      }
    }
  }
}

// ============================================================================
// CHARACTER PROCESSING FUNCTION
// ============================================================================
void addchar(void) {
  // Check if cursor is in delete key area (bottom-right corner)
  if (my > KEY_AREA_Y) {
    // Delete last character from string (if any)
    if (text.length() > 0) {
      text = text.substring(0, text.length() - 1);
    }
    
    // Update displayed string (with trailing space to clear)
    tftCursor(60, 60);
    tftPrint(text);
    tftPrint(" ");
    
    // Output to serial for debugging/use
  Serial_Println("String: " + text);
  } else {
    // Cursor is in keyboard area - add selected character
    byte charPosition = mx / 15 + (my / 19) * 19;
    char selectedChar = keyboardChars[charPosition];
    
    // Check for ENTER/SEND character (0x7B = '{')
    // When '{' is selected, the text turns green indicating it's ready
    // for pickup by other parts of the program
    if (selectedChar == 0x7B) {
      setTextCol(GN, BK);  // Change text color to green (ENTER/SEND indicator)
    Serial_Println("ENTER pressed - String ready: " + text);
    } else {
      // Add character to string if not at maximum length
      if (text.length() < MAX_STRING_LEN) {
        text += selectedChar;
      }
    }
    
    // Update displayed string
    tftCursor(60, 60);
    tftPrint(text);
  }
  
  // Reset text color to default (white) after processing
  setTextCol(WH, BK);
  
  // Debounce - wait for button release
  while (!digitalRead(PC1)) delay(10);
}

// ============================================================================
// MOUSE POSITION UPDATING FUNCTION
// ============================================================================
void mousPos(void) {
  static int prevX, prevY;  // Previous cursor position
  
  // Read mouse movement deltas (signed 8-bit values)
  int8_t deltaX = readPAW(0x03);  // X movement: -128 to 127
  int8_t deltaY = readPAW(0x04);  // Y movement: -128 to 127
  
  // Erase previous cursor highlight
  if (my < KEY_AREA_Y) {
    drawRect(prevX, prevY, 14, 18, BK);
  } else {
    drawRect(256, 59, 26, 15, BK);
    drawRect(255, 58, 28, 17, BK);
  }
  
  // Update cursor position with scaling and Y-axis inversion
  // Scaling factor 3 reduces sensitivity for precise control
  msx = msx + deltaX / 3;
  msy = msy - deltaY / 3;  // Negative because display Y increases downward
  
  // Apply screen boundaries
  if (msx < 2) msx = 2;
  if (msy < 2) msy = 2;
  if (msx > 280) msx = 280;  // Right edge of display
  
  // Special handling for delete key area
  byte yLimit = KEY_AREA_Y;  // Default limit
  if (msx > 260) yLimit = 60;  // Allow lower Y near delete key
  if (msy > yLimit) msy = yLimit;
  
  // Convert continuous position to grid-aligned position
  mx = (msx / 15) * 15;      // Snap to 15-pixel column grid
  my = (msy / 19) * 19 + 1;  // Snap to 19-pixel row grid
  
  // Draw new cursor highlight
  if (my < KEY_AREA_Y) {
    drawRect(mx, my, 14, 18, GN);
    drawRect(mx - 1, my - 1, 16, 20, GN);
  } else {
    drawRect(256, 59, 26, 15, RD);
    drawRect(255, 58, 28, 17, RD);
  }
  
  // Save current position for next update
  prevX = mx;
  prevY = my;
}

// ============================================================================
// INTERFACE DRAWING FUNCTION
// ============================================================================
void graph(void) {
  // Draw outer border
  drawRect(0, 0, 284, 76, YL);
  
  // Draw vertical grid lines (keyboard columns)
  for (int i = 14; i < 284; i += 15) {
    drawLine(i, 0, i, 55, YL);
    if (!(GPIOC->INDR & (1 << 1))) break;  // Exit if button pressed
  }
  
  // Draw horizontal grid lines (keyboard rows)
  for (int i = 19; i < 60; i += 19) {
    drawLine(0, i, 283, i, YL);
    if (!(GPIOC->INDR & (1 << 1))) break;  // Exit if button pressed
  }
}

// ============================================================================
// PAW3205 MOUSE SENSOR FUNCTIONS
// ============================================================================
// Note: PAW3205 uses a custom 2-wire SPI-like protocol

// Resynchronize mouse sensor serial interface
void resync(void) {
  // Generate sync pulse as per PAW3205 datasheet section 6.2
  digitalWrite(MOUSE_SCLK, 1);
  digitalWrite(MOUSE_SCLK, 0);
  delayMicroseconds(2);  // t_RESYNC minimum = 1µs
  digitalWrite(MOUSE_SCLK, HIGH);
  delay(20);             // Wait for watchdog timer timeout (t_SIWTT min = 1.7ms)
}

// Send one byte to PAW3205 (MSB first)
void sendByte(byte data) {
  pinMode(MOUSE_SDIO, OUTPUT);
  byte mask = 0x80;
  
  for (int i = 0; i < 8; i++) {
    digitalWrite(MOUSE_SCLK, LOW);
    
    // Output MSB first
    if (data & mask) 
      digitalWrite(MOUSE_SDIO, 1);
    else  
      digitalWrite(MOUSE_SDIO, 0);
    
    digitalWrite(MOUSE_SCLK, HIGH);
    mask = mask / 2;  // Shift right for next bit
  }
}

// Receive one byte from PAW3205 (MSB first)
byte receiveByte() {
  pinMode(MOUSE_SDIO, INPUT_PULLUP);  // Switch to input mode
  delayMicroseconds(5);                // t_HOLD minimum = 3µs
  
  byte data = 0, mask = 0x80;
  
  for (int i = 0; i < 8; i++) {
    digitalWrite(MOUSE_SCLK, LOW);
    digitalWrite(MOUSE_SCLK, HIGH);
    
    if (digitalRead(MOUSE_SDIO)) 
      data |= mask;
    
    mask = mask / 2;  // Shift right for next bit
  }
  
  return data;
}

// Write to PAW3205 register
void writePAW(byte reg, byte data) {
  sendByte(reg | 0x80);  // MSB=1 for write operation
  sendByte(data);
  delayMicroseconds(10);  // Ensure proper timing
}

// Read from PAW3205 register
byte readPAW(byte reg) {
  sendByte(reg & 0x7F);  // MSB=0 for read operation
  
  // Release data line for sensor to drive
  pinMode(MOUSE_SDIO, INPUT_PULLUP);
  delayMicroseconds(5);  // t_HOLD minimum = 3µs
  
  return receiveByte();
}

// ============================================================================
// DISPLAY CONTROL FUNCTIONS
// ============================================================================

// Send command byte to display
void writeCommand(uint8_t cmd) {
  GPIOD->OUTDR &= ~(1 << 3);  // DC LOW = command mode
  GPIOD->OUTDR &= ~(1 << 2);  // CS LOW = select display
  SPI1_Transfer(cmd);
  GPIOD->OUTDR |= (1 << 2);   // CS HIGH = deselect display
}

// Send data byte to display
void writeData(uint8_t data) {
  GPIOD->OUTDR |= (1 << 3);   // DC HIGH = data mode
  GPIOD->OUTDR &= ~(1 << 2);  // CS LOW = select display
  SPI1_Transfer(data);
  GPIOD->OUTDR |= (1 << 2);   // CS HIGH = deselect display
}

// SPI data transfer function (hardware SPI1)
void SPI1_Transfer(uint8_t data) {
  // Wait for transmit buffer empty
  while (!(SPI1->STATR & (1 << 1)));
  
  SPI1->DATAR = data;
  
  // Wait for receive buffer not empty
  while (!(SPI1->STATR & (1 << 0)));
  
  // Clear RX buffer (important for proper operation)
  volatile uint8_t dummy __attribute__((unused)) = SPI1->DATAR;
}

// Set display address window with display offsets
void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  // Apply display-specific offsets
  x += xStart;
  y += yStart;
  
  // Combine start and end addresses into 32-bit values
  uint32_t xAddr = ((uint32_t)x << 16) | (x + w);
  uint32_t yAddr = ((uint32_t)y << 16) | (y + h);

  // Set column address range
  writeCommand(0x2A);  // Column Address Set
  writeData(xAddr >> 24);
  writeData(xAddr >> 16);
  writeData(xAddr >> 8);
  writeData(xAddr & 0xFF);
  
  // Set row address range
  writeCommand(0x2B);  // Row Address Set
  writeData(yAddr >> 24);
  writeData(yAddr >> 16);
  writeData(yAddr >> 8);
  writeData(yAddr & 0xFF);
  
  // Prepare for memory write
  writeCommand(0x2C);  // Memory Write
}

// Set display rotation/orientation
void setRotation(uint8_t rotation) {
  rotate = rotation % 4;
  uint8_t madctl;
  
  switch (rotate) {
    case 0:  // Portrait (0°)
      madctl = 0xC0;  // MY=1, MX=1, MV=0
      xStart = TFT_X_OFFSET;
      yStart = TFT_Y_OFFSET;
      break;
    case 1:  // Landscape (90° clockwise)
      madctl = 0xB0;  // MY=1, MX=0, MV=1
      xStart = TFT_Y_OFFSET;
      yStart = TFT_X_OFFSET;
      break;
    case 2:  // Portrait (180°)
      madctl = 0x00;  // MY=0, MX=0, MV=0
      xStart = TFT_X_OFFSET;
      yStart = TFT_Y_OFFSET;
      break;
    case 3:  // Landscape (270° clockwise)
      madctl = 0x60;  // MY=0, MX=1, MV=1
      xStart = TFT_Y_OFFSET;
      yStart = TFT_X_OFFSET;
      break;
  }
  
  writeCommand(0x36);  // MADCTL command
  writeData(madctl);
  delay(10);
}

// ============================================================================
// BASIC DRAWING FUNCTIONS
// ============================================================================

// Draw single pixel at specified coordinates
void tftPixel(uint16_t x, uint16_t y, uint16_t color) {
  setAddrWindow(x, y, 1, 1);  // Set window to single pixel
  
  GPIOD->OUTDR |= (1 << 3);    // DC HIGH = data mode
  GPIOD->OUTDR &= ~(1 << 2);   // CS LOW = select display
  
  // Send 16-bit color (RGB565)
  SPI1_Transfer(color >> 8);    // High byte
  SPI1_Transfer(color & 0xFF);  // Low byte
  
  GPIOD->OUTDR |= (1 << 2);    // CS HIGH = deselect display
}

// Fill entire screen with specified color
void fillScreen(uint16_t color) {
  uint8_t hi = color >> 8, lo = color & 0xFF;
  
  // Set address window for entire display (adjust for rotation)
  if (rotate == 0 || rotate == 2) {
    // Portrait orientation
    setAddrWindow(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  } else {
    // Landscape orientation
    setAddrWindow(0, 0, DISPLAY_HEIGHT, DISPLAY_WIDTH);
  }
  
  GPIOD->OUTDR |= (1 << 3);    // DC HIGH = data mode
  GPIOD->OUTDR &= ~(1 << 2);   // CS LOW = select display
  
  // Fill all pixels
  for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
    SPI1_Transfer(hi);
    SPI1_Transfer(lo);
  }
  
  GPIOD->OUTDR |= (1 << 2);    // CS HIGH = deselect display
}

// ============================================================================
// TEXT FUNCTIONS
// ============================================================================

// Set text size multiplier (1-4)
void setTextSize(byte x) {
  if (x < 1) x = 1;
  if (x > 4) x = 4;
  textSize = x;
}

// Set text and background colors
void setTextCol(int c, int b) {
  textCol = c;
  textBgr = b;
}

// Set text cursor position
void tftCursor(uint16_t tftx, uint16_t tfty) {
  tftIdx = tftx;
  tftIdy = tfty;
}

// Draw single character at specified position
void tftChar(uint16_t x, uint16_t y, char c) {
  // Only draw printable ASCII characters (32-127)
  if (c < 32 || c > 127) return;
  
  // Get font data for this character (5 bytes per character)
  const uint8_t *charData = &Chartable[(c - 32) * 5];
  
  for (uint8_t col = 0; col < 5; col++) {
    uint8_t pixels = charData[col];
    for (uint8_t row = 0; row < 7; row++) {
      bool pixelOn = pixels & (1 << row);
      
      if (pixelOn || textBgr != -1) {
        for (uint8_t sx = 0; sx < textSize; sx++) {
          for (uint8_t sy = 0; sy < textSize; sy++) {
            uint16_t px = x + (col * textSize) + sx;
            uint16_t py = y + (row * textSize) + sy;
            uint16_t pixelColor = pixelOn ? textCol : (uint16_t)textBgr; 
            tftPixel(px, py, pixelColor);
          }
        }
      }
    }
  }
}

// Common border checking function for text wrapping
void tftCheckNewline() {
  if (rotate == 0 || rotate == 2) {
    // Portrait mode width (adjust as needed)
    if (tftIdx >= DISPLAY_WIDTH - textSize * 6) { 
      tftIdx = 0; 
      tftIdy += 8 * textSize; 
    }
  } else {
    // Landscape mode width (adjust as needed)
    if (tftIdx >= DISPLAY_HEIGHT - textSize * 6) { 
      tftIdx = 0; 
      tftIdy += 8 * textSize; 
    }
  }
}

// Print long (signed 32-bit)
void tftPrint(long n) {
  char buffer[12];  // Enough for -2147483648
  char *p = buffer + 11;
  *p = '\0';
  
  if (n == 0) {
    *--p = '0';
  } else {
    bool negative = n < 0;
    if (negative) n = -n;
    
    while (n > 0) {
      *--p = '0' + (n % 10);
      n /= 10;
    }
    if (negative) *--p = '-';
  }
  
  // Call existing string print function
  while (*p) {
    tftChar(tftIdx, tftIdy, *p++);
    tftIdx += 6 * textSize;
    tftCheckNewline();
  }
}

// Print unsigned long (unsigned 32-bit)
void tftPrint(unsigned long n) {
  char buffer[11];  // Enough for 4294967295
  char *p = buffer + 10;
  *p = '\0';
  
  if (n == 0) {
    *--p = '0';
  } else {
    while (n > 0) {
      *--p = '0' + (n % 10);
      n /= 10;
    }
  }
  
  // Call existing string print function
  while (*p) {
    tftChar(tftIdx, tftIdy, *p++);
    tftIdx += 6 * textSize;
    tftCheckNewline();
  }
}

// Function to handle single characters
void tftPrint(char c) {
  tftChar(tftIdx, tftIdy, c);
  tftIdx += 6 * textSize;
  tftCheckNewline();
}

// Print C string
void tftPrint(char *p) {
  while (*p) {
    tftChar(tftIdx, tftIdy, *p++);
    tftIdx += 6 * textSize;
    tftCheckNewline();
  }
}

// String version
void tftPrint(const String &str) {
  for (uint16_t i = 0; i < str.length(); i++) {
    tftChar(tftIdx, tftIdy, str.charAt(i));
    tftIdx += 6 * textSize;
    tftCheckNewline();
  }
}

// Print signed integer (int)
void tftPrint(int n) {
  char buffer[12];
  char *p = buffer + 11;
  *p = '\0';
  
  if (n == 0) {
    *--p = '0';
  } else {
    bool negative = n < 0;
    if (negative) n = -n;
    
    while (n > 0) {
      *--p = '0' + (n % 10);
      n /= 10;
    }
    if (negative) *--p = '-';
  }
  
  while (*p) {
    tftChar(tftIdx, tftIdy, *p++);
    tftIdx += 6 * textSize;
    tftCheckNewline();
  }
}

// Print unsigned integer (uint16_t, uint32_t, etc.)
void tftPrint(unsigned int n) {
  char buffer[11];
  char *p = buffer + 10;
  *p = '\0';
  
  if (n == 0) {
    *--p = '0';
  } else {
    while (n > 0) {
      *--p = '0' + (n % 10);
      n /= 10;
    }
  }
  
  while (*p) {
    tftChar(tftIdx, tftIdy, *p++);
    tftIdx += 6 * textSize;
    tftCheckNewline();
  }
}

// Print int8_t (signed byte)
void tftPrint(int8_t n) {
  tftPrint((int)n);
}

// Print uint8_t (unsigned byte)
void tftPrint(uint8_t n) {
  tftPrint((unsigned int)n);
}

// ============================================================================
// GRAPHICS FUNCTIONS
// ============================================================================
#define swap(a, b) { int16_t t = a; a = b; b = t; }

// Draw line between two points (Bresenham algorithm)
void drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
  int dx, dy, err, ystep;
  int steep = abs(y1 - y0) > abs(x1 - x0);
  
  if (steep) { swap(x0, y0); swap(x1, y1); }
  if (x0 > x1) { swap(x0, x1); swap(y0, y1); }
  
  dx = x1 - x0;
  dy = abs(y1 - y0);
  err = dx / 2;
  
  if (y0 < y1) { ystep = 1; } 
  else { ystep = -1; }
  
  for (; x0 <= x1; x0++) {
    if (steep) {
      tftPixel(y0, x0, color);
    } else {
      tftPixel(x0, y0, color);
    }
    err -= dy;
    if (err < 0) { y0 += ystep; err += dx; }
  }
}

// Draw horizontal line
void hLine(int x, int y, int w, uint16_t color) {
  drawLine(x, y, x + w-1, y, color);
}

// Draw vertical line
void vLine(int x, int y, int h, uint16_t color) {
  drawLine(x, y, x, y + h-1, color);
}

// Draw rectangle outline
void drawRect(int x, int y, int w, int h, uint16_t color) {
  hLine(x, y, w, color);
  hLine(x, y + h - 1, w, color);
  vLine(x, y, h, color);
  vLine(x + w - 1, y, h, color);
}

// Draw filled rectangle
void fillRect(int x, int y, int w, int h, uint16_t color) {
  for (int i = x; i < x + w; i++) {
    vLine(i, y, h, color);
  }
}

// Draw circle outline (midpoint circle algorithm)
void drawCircle(int x0, int y0, int r, uint16_t color) {
  int f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
  
  tftPixel(x0, y0 + r, color);
  tftPixel(x0, y0 - r, color);
  tftPixel(x0 + r, y0, color);
  tftPixel(x0 - r, y0, color);
  
  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    
    tftPixel(x0 + x, y0 + y, color);
    tftPixel(x0 - x, y0 + y, color);
    tftPixel(x0 + x, y0 - y, color);
    tftPixel(x0 - x, y0 - y, color);
    tftPixel(x0 + y, y0 + x, color);
    tftPixel(x0 - y, y0 + x, color);
    tftPixel(x0 + y, y0 - x, color);
    tftPixel(x0 - y, y0 - x, color);
  }
}

// Draw filled circle
void fillCircle(int x0, int y0, int r, uint16_t color) {
  vLine(x0, y0 - r, 2 * r + 1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

// Helper function for filled circles
void fillCircleHelper(int x0, int y0, int r, char cornername, int delta, uint16_t color) {
  int f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
  
  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    
    if (cornername & 0x1) {
      vLine(x0 + x, y0 - y, 2 * y + 1 + delta, color);
      vLine(x0 + y, y0 - x, 2 * x + 1 + delta, color);
    }
    if (cornername & 0x2) {
      vLine(x0 - x, y0 - y, 2 * y + 1 + delta, color);
      vLine(x0 - y, y0 - x, 2 * x + 1 + delta, color);
    }
  }
}

// ============================================================================
// DISPLAY INITIALIZATION FUNCTION
// ============================================================================
void InitTFT(void) {
  // Initialize pins
  GPIOD->OUTDR |= (1 << 2);    // CS HIGH (idle)
  GPIOD->OUTDR |= (1 << 4);    // RST HIGH (inactive)
  
  // Hardware reset
  GPIOD->OUTDR &= ~(1 << 4);   // RST LOW
  delay(20);
  GPIOD->OUTDR |= (1 << 4);    // RST HIGH
  delay(150);
  
  // ST7789 Initialization Sequence
  writeCommand(0x01);   // Software reset
  delay(150);
  
  writeCommand(0x11);   // Sleep out
  delay(255);
  
  writeCommand(0x36);   // Memory Data Access Control
  writeData(0xC0);      // MX=1, MY=1, RGB mode
  
  writeCommand(0x3A);   // Interface Pixel Format
  writeData(0x55);      // 16-bit color (RGB565)
  delay(10);
  
  // Set display area (76x284)
  writeCommand(0x2A);   // Column Address Set
  writeData(0x00);
  writeData(0x00);      // Start column = 0
  writeData(0x00);
  writeData(0xEF);      // End column = 239 (0xEF)
  
  writeCommand(0x2B);   // Row Address Set
  writeData(0x00);
  writeData(0x00);      // Start row = 0
  writeData(0x01);
  writeData(0x3F);      // End row = 319 (0x13F)
  
  writeCommand(0x20);   // Display Inversion off
  delay(10);
  
  writeCommand(0x29);   // Display On
  delay(100);
  
  // Clear screen
  fillScreen(BK);
}

// ============================================================================
// HARDWARE UART FUNCTIONS
// ============================================================================

// Send a single character via hardware UART
void Serial_Write(char c) {
  while (!(USART1->STATR & USART_STATR_TXE));  // Wait for TX empty
  USART1->DATAR = c;
}

// Send a string via hardware UART
void Serial_Print(const char* str) {
  while (*str) {
    Serial_Write(*str++);
  }
}

// Send a string with newline via hardware UART
void Serial_Println(const char* str) {
 Serial_Print(str);
  Serial_Write('\r');
  Serial_Write('\n');
}

// Send Arduino String object via hardware UART
void Serial_Print(const String &str) {
  for (uint16_t i = 0; i < str.length(); i++) {
    Serial_Write(str.charAt(i));
  }
}

// Send Arduino String object with newline via hardware UART
void Serial_Println(const String &str) {
 Serial_Print(str);
  Serial_Write('\r');
  Serial_Write('\n');
}

// Send integer via hardware UART
void Serial_Print(int n) {
  char buffer[12];
  char *p = buffer + 11;
  *p = '\0';
  
  if (n == 0) {
    *--p = '0';
  } else {
    bool negative = n < 0;
    if (negative) n = -n;
    
    while (n > 0) {
      *--p = '0' + (n % 10);
      n /= 10;
    }
    if (negative) *--p = '-';
  }
  
 Serial_Print(p);
}

// Send integer with newline via hardware UART
void Serial_Println(int n) {
 Serial_Print(n);
  Serial_Write('\r');
  Serial_Write('\n');
}

// Check if data is available to read
bool Serial_Available() {
  return (USART1->STATR & USART_STATR_RXNE);
}

// Read a single character
char Serial_Read() {
  while (!Serial_Available());  // Wait for data
  return USART1->DATAR;
}