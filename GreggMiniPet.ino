//--------------------------------------------------------------------------
// Animated Commodore PET plaything. Uses the following parts:
//   - Feather M0 microcontroller (adafruit.com/products/2772)
//   - 9x16 CharliePlex matrix (2972 is green, other colors avail.)
//   - Optional LiPoly battery (1578) and power switch (805)
//
// This is NOT good "learn from" code for the IS31FL3731. Taking a cue from
// our animated flame pendant project, this code addresses the CharliePlex
// driver chip directly to achieve smooth full-screen animation.  If you're
// new to graphics programming, download the Adafruit_IS31FL3731 and
// Adafruit_GFX libraries, with examples for drawing pixels, lines, etc.
//
// Animation cycles between different effects: typing code, Conway's Game
// of Life, The Matrix effect, and a blank screen w/blinking cursor (shown
// for a few seconds before each of the other effects; to imply "loading").
//--------------------------------------------------------------------------

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_IS31FL3731.h>
#include "Melody.h"
#include "Entertainer.h"

// If you're using the full breakout...
Adafruit_IS31FL3731 matrix = Adafruit_IS31FL3731();

#define WIDTH      16        // Matrix size in pixels
#define HEIGHT      9
#define GAMMA     2.5        // Gamma-correction exponent

uint8_t img[WIDTH * HEIGHT]; // 8-bit buffer for image rendering
uint8_t bitmap[((WIDTH+7)/8) * HEIGHT]; // 1-bit buffer for some modes
uint8_t gamma8[256];         // Gamma correction (brightness) table
uint8_t page  = 0;           // Double-buffering front/back control
uint8_t frame = 0;           // Frame counter used by some animation modes
uint8_t fadein = 0;          // Used to fade in an image
// More globals later, above code for each animation, and before setup()


// UTILITY FUNCTIONS -------------------------------------------------------

// Set bit at (x,y) in the bitmap buffer (no clear function, wasn't needed)
void bitmapSetPixel(int8_t x, int8_t y) {
  bitmap[y * ((WIDTH + 7) / 8) + x / 8] |= 0x80 >> (x & 7);
}

// Read bit at (x,y) in bitmap buffer, returns nonzero (not always 1) if set
uint8_t bitmapGetPixel(int8_t x, int8_t y) {
  return bitmap[y * ((WIDTH + 7) / 8) + x / 8] & (0x80 >> (x & 7));
}


// BLINKING CURSOR / LOADING EFFECT ----------------------------------------
// Minimal animation - just one pixel in the corner blinks on & off,
// meant to suggest "program loading" or similar busy effect.

void cursorLoop() {
  int16_t x = 0;
  int16_t y = 1;
  img[y*WIDTH+x] = (frame & 1) * 255;
}


// TERMINAL TYPING EFFECT --------------------------------------------------
// I messed around trying to make a random "fake code generator," but it
// was getting out of hand. Instead, the typed "code" is just a bitmap!

const uint16_t codeBits[] = {
  0b1110111110100000,
  0b0011101100000000,
  0b0011011111101000,
  0b0000111111011100,
  0b0000111011100000,
  0b0010000000000000,
  0b0011011111000000,
  0b1000000000000000,
  0b0000000000000000,
  0b1111011010000000,
  0b0011110111110110,
  0b1000000000000000,
  0b0000000000000000,
  0b1110111101000000,
  0b0011101011010000,
  0b0011110111111000,
  0b0011101101110000,
  0b0011011111101000,
  0b0000111011111100,
  0b0010000000000000,
  0b1000000000000000,
  0b0000000000000000,
  0b1110110100000000,
  0b0011011111110100,
  0b0000111101100000,
  0b0010000000000000,
  0b0011110111110000,
  0b0011101111011000,
  0b1000000000000000,
  0b0000000000000000
};

uint8_t cursorX, cursorY, line;

void typingSetup() {
  cursorX = cursorY = line = 0;
}

void typingLoop() {
  img[cursorY * WIDTH + cursorX] = // If bit set, "type" random char
    ((codeBits[line] << cursorX) & 0x8000) ? random(32, 128) : 0;

  cursorX++;
  if(!(uint16_t)(codeBits[line] << cursorX)) { // End of line reached?
    cursorX = 0;
    if(cursorY >= HEIGHT-1) { // Cursor on last line?
      uint8_t y;
      for(y=0; y<HEIGHT-1; y++) // Move img[] buffer up one line
        memcpy(&img[y * WIDTH], &img[(y+1) * WIDTH], WIDTH);
      memset(&img[y * WIDTH], 0, WIDTH); // Clear last line
    }
    else {
      cursorY++;
    }
    if(++line >= (sizeof(codeBits) / sizeof(codeBits[0]))) {
      line = 0;
    }
  }
  img[cursorY * WIDTH + cursorX] = 255; // Draw cursor in new position
}


// MATRIX EFFECT -----------------------------------------------------------
// Inspired by "The Matrix" coding effect -- 'raindrops' travel down the
// screen, their 'tails' twinkle slightly and fade out.

#define N_DROPS 15
struct {
  int8_t  x, y; // Position of raindrop 'head'
  uint8_t len;  // Length of raindrop 'tail' (not incl head)
} drop[N_DROPS];

void matrixRandomizeDrop(uint8_t i) {
  drop[i].x   = random(WIDTH);
  drop[i].y   = random(-18, 0);
  drop[i].len = random(9, 18);
}

void matrixSetup() {
  for(uint8_t i=0; i<N_DROPS; i++) matrixRandomizeDrop(i);
}

void matrixLoop() {
  uint8_t i, j;
  int8_t  y;

  for(i=0; i<N_DROPS; i++) { // For each raindrop...
    // If head is onscreen, overwrite w/random brightness 20-80
    if((drop[i].y >= 0) && (drop[i].y < HEIGHT)) {
      img[drop[i].y * WIDTH + drop[i].x] = random(20, 80);
    }
    // Move pos. down by one. If completely offscreen (incl tail), make anew
    if((++drop[i].y - drop[i].len) >= HEIGHT) {
      matrixRandomizeDrop(i);
    }
    for(j=0; j<drop[i].len; j++) {     // For each pixel in drop's tail...
      y = drop[i].y - drop[i].len + j; // Pixel Y coord
      if((y  >= 0) && (y < HEIGHT)) {  // On screen?
        // Make 4 pixels at end of tail fade out.  For other tail pixels,
        // there's a 1/10 chance of random brightness change 20-80
        if(j < 4) {
          img[y * WIDTH + drop[i].x] /= 2;
        }
        else if(!random(10)) {
          img[y * WIDTH + drop[i].x] = random(20, 80);
        }
      }
    }
    if((drop[i].y >= 0) && (drop[i].y < HEIGHT)) { // If head is onscreen,
      img[drop[i].y * WIDTH + drop[i].x] = 255;    // draw w/255 brightness
    }
  }
}


// CONWAY'S GAME OF LIFE ---------------------------------------------------
// The rules: if cell at (x,y) is currently populated, it stays populated
// if it has 2 or 3 populated neighbors, else is cleared.  If cell at (x,y)
// is currently empty, populate it if 3 neighbors.

void lifeSetup() { // Fill bitmap with random data
  for(uint8_t i=0; i<sizeof(bitmap); i++) {
    bitmap[i] = random(256);
  }
}

void lifeLoop() {
  static const int8_t xo[] = { -1,  0,  1, -1, 1, -1, 0, 1 },
                      yo[] = { -1, -1, -1,  0, 0,  1, 1, 1 };
  int8_t              x, y;
  uint8_t             i, n;

  // Modify img[] based on old contents (dimmed) + new bitmap
  for(i=y=0; y<HEIGHT; y++) {
    for(x=0; x<WIDTH; x++, i++) {
      if(bitmapGetPixel(x, y)) img[i]  = 255;
      else if(img[i] > 28)     img[i] -= 28;
      else                     img[i]  = 0;
    }
  }

  // Generate new bitmap (next frame) based on img[] contents + rules
  memset(bitmap, 0, sizeof(bitmap));
  for(y=0; y<HEIGHT; y++) {
    for(x=0; x<WIDTH; x++) {
      for(i=n=0; (i < sizeof(xo)) && (n < 4); i++)
        n += (img[((y+yo[i])%HEIGHT) * WIDTH + ((x+xo[i])%WIDTH)] == 255);
      if((n == 3) || ((n == 2) && (img[y * WIDTH + x] == 255)))
        bitmapSetPixel(x, y);
    }
  }

  // Every 32 frames, populate a random cell so animation doesn't stagnate
  if(!(frame & 0x1F)) bitmapSetPixel(random(WIDTH), random(HEIGHT));
}


// HELLO WORLD ---------------------------------------------------
// Print Hello World

void helloWorldLoop() {
  matrix.setTextSize(1);
  matrix.setTextWrap(false);  // we dont want text to wrap so it scrolls nicely
  matrix.setTextColor(100);
  for (int8_t x=14; x>=-46; x--) {
    matrix.clear();
    matrix.setCursor(x,1);
    matrix.print("Hello");
    delay(100);
  }

  matrix.setTextSize(1);
  matrix.setTextWrap(false);  // we dont want text to wrap so it scrolls nicely
  matrix.setTextColor(32);
  matrix.setRotation(1);
  for (int8_t x=7; x>=-40; x--) {
    matrix.clear();
    matrix.setCursor(x,4);
    matrix.print("World");
    delay(100);
  }
}

// FORD OVAL -----------------------------------------------------------

static const uint16_t PROGMEM oval_bmp[] = {
  0b0001111111111000,
  0b0010000000000100,
  0b0100000000000010,
  0b1000000000000001,
  0b1000000000000001,
  0b1000000000000001,
  0b0100000000000010,
  0b0010000000000100,
  0b0001111111111000,
};

static const uint16_t PROGMEM ford_bmp[] = {
  0b0000000000000000,
  0b0000000000000000,
  0b0011100000000100,
  0b0010000000000100,
  0b0011001101101100,
  0b0010010101010100,
  0b0010011001011100,
  0b0000000000000000,
  0b0000000000000000,
};

uint8_t oval_fadein;
uint8_t ford_fadein;

void fordSetup() {
  matrix.clear();
  oval_fadein = 0;
  ford_fadein = 0;
}

void fordLoop() {
  int8_t i = 0;
  if (oval_fadein < 255-16 && ford_fadein == 0) {
    oval_fadein += 16;
  }
  else {
    oval_fadein -= 8;
    if (ford_fadein < 255-16) {
      ford_fadein += 16;
    }
    else {
      ford_fadein = 255;
      oval_fadein = 96;
    }
  }
  for (int16_t y=0; y<HEIGHT; y++){
    uint16_t row = oval_bmp[y];
    for (int16_t x=0; x<WIDTH; x++) {
      if (row & (1 << (WIDTH-x-1))) {
        img[y*WIDTH+x] = oval_fadein;
      }
    }
  }
  if (ford_fadein > 0) {
    for (int16_t y=0; y<HEIGHT; y++){
      uint16_t row = ford_bmp[y];
      for (int16_t x=0; x<WIDTH; x++) {
        if (row & (1 << (WIDTH-x-1))) {
          img[y*WIDTH+x] = ford_fadein;
        }
      }
    }
  }
}


// Star Wars -----------------------------------------------------------

static const uint16_t PROGMEM star_wars_crawl[] = {
  0b0000001110100000,
  0b0000010110110000,
  0b0000000000000000,
  0b1010111010111011,
  0b1110111111011110,
  0b1101011101101101,
  0b1101101111011110,
  0b1011011110111100,
  0b0000000000000000,
  0b1110101110111000,
  0b1110111010110111,
  0b1110101011110000,
  0b1111011101101110,
  0b1110101110111000,
  0b1111011011101110,
  0b1011110101110111,
  0b0000000000000000,
  0b1111010110111100,
  0b1111011110111100,
  0b1101101101110110,
  0b1111011111010110,
  0b1110110110101101,
  0b1110110111100000,
  0b1111010101110101
};
#define STARWARS_LINES sizeof(star_wars_crawl)/sizeof(uint16_t)

uint8_t sw_index;
uint8_t sw_line;
void starwarsSetup() {
  sw_index = 0;
  sw_line = HEIGHT-1;
}

void starwarsLoop() {
  uint16_t row_bits = 0;
  if (sw_index < STARWARS_LINES) {
    row_bits = star_wars_crawl[sw_index];
  }
  sw_index++;
  if(sw_line >= HEIGHT-1) { // Cursor on last line?
    uint8_t y = 0;
    for(y=1; y<HEIGHT-1; y++) { // Move img[] buffer up one line
      uint8_t taper = (WIDTH/2)-y;
      uint8_t dot_minus;
      uint8_t dot;
      uint8_t dot_plus;
      uint8_t x;
      memset(&img[y * WIDTH], 0, WIDTH); // Clear new line

      // set the dots
      for (x=taper; x<WIDTH-taper; x++) {
        dot_minus = img[(y+1)*WIDTH+x-1];
        dot = img[(y+1)*WIDTH+x];
        dot_plus = img[(y+1)*WIDTH+x+1];
        if ((dot_minus < 128 && dot < 128) || (dot < 128 && dot_plus < 128) || (dot_minus < 128 && dot_plus < 128)) {
          img[y*WIDTH+x] = 0;
        }
        else if ((dot_minus > 128 && dot > 128 && dot_plus > 128)) {
          img[y*WIDTH+x] = 255;
        }
        else {
          dot_minus = dot_minus/3;
          dot = dot/3;
          dot_plus = dot_plus/3;
          img[y*WIDTH+x] = dot_minus + dot + dot_plus;
        }
      }
    }
    memset(&img[y * WIDTH], 0, WIDTH); // Clear last line
  }
  else {
    sw_line++;
  }
  // Draw the new line as is
  for (int16_t x=0; x<WIDTH; x++) {
    if (row_bits & (1 << (WIDTH-x-1))) {
      img[sw_line*WIDTH+x] = 255;
    }
  }
}


// Entertainer -----------------------------------------------------------

const int speakerPin = 9;
int thisNote=0;
int pauseBetweenNotes = 0;
uint32_t pauseBegin = 0;
int scaleArray[WIDTH];

void entertainerSetup() {
  // Need the speaker pin to defined as OUTPUT
  pinMode(speakerPin, OUTPUT);
  thisNote = 0;
  pauseBetweenNotes = 0;
  pauseBegin = millis();
  memset(scaleArray, 0, sizeof(scaleArray));  // Clear the scales
}

void entertainerLoop() {
  playMusic(speakerPin,entertainer);
}

void playMusic(int speakerPin, Note music[]) {
    // iterate over the notes of the melody:
    while (music[thisNote].note != END_NOTES) {
      playNote(speakerPin, music[thisNote]);
      thisNote++;
    }
    noTone(speakerPin);
}

void playNote(int speakerPin, Note currentNote) {

  clearImg();

  appendScale(currentNote.scale);
  for (int n = 0; n <=WIDTH; n++) {
    if (scaleArray[n] != 0) {  // Not a Rest
      int16_t x = WIDTH;
      int16_t y = scaleArray[n];
      img[y*WIDTH+x] = 255;
    }
  }
  copyImg();
  displayAndFlipPage();

  // Pause moved to beginning so display can send the next note just before
  // it is played.
  while((millis() - pauseBegin) < pauseBetweenNotes);

  // to calculate the note duration, take one second 
  // divided by the note type.
  //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
  int noteDuration = 1000/currentNote.duration;
  tone(speakerPin, currentNote.note, noteDuration);
  
  // to distinguish the notes, set a minimum time between them.
  // the note's duration + 30% seems to work well:
  pauseBegin = millis();
  pauseBetweenNotes = noteDuration * 1.3;
  
}

void appendScale(int scale) {
  for (int s=0; s<WIDTH-1; s++) {
    scaleArray[s] = scaleArray[s+1];
  }
  scaleArray[WIDTH-1] = scale;
}


// MORE GLOBAL STUFF - ANIMATION STATES ------------------------------------

struct { // For each of the animation modes...
  void    (*setup)(void); // Animation setup func (run once on mode change)
  void    (*loop)(void);  // Animation loop func (renders one frame)
  uint8_t maxRunTime;     // Animation run time in seconds
  uint8_t fps;            // Frames-per-second for this effect
} anim[] = {
  NULL,          cursorLoop,     3,   4,
  typingSetup,   typingLoop,     15, 15,
  lifeSetup,     lifeLoop,       12, 30,
  matrixSetup,   matrixLoop,     15, 10,
  NULL,          helloWorldLoop, 1,   2,
  fordSetup,     fordLoop,       15,  4,
  starwarsSetup, starwarsLoop,   (STARWARS_LINES+HEIGHT)/2,  2,
  entertainerSetup, entertainerLoop, 5,  1,
};

uint8_t  seq[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6}; // Sequence of animation modes
//uint8_t  seq[] = { 0, 7, 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6}; // Sequence of animation modes
//uint8_t  seq[] = { 6, 0, 6, 0, 6, 0, 6, 0, 6, 0, 6}; // Sequence of animation modes
uint8_t  idx   = sizeof(seq) - 1;      // Current position in seq[]
//uint8_t  idx   = 0;      // Current position in seq[]
uint32_t modeStartTime = 0x7FFFFFFF;   // micros() when current mode started


// SETUP - RUNS ONCE AT PROGRAM START --------------------------------------

void setup() {
  uint16_t i;
  uint8_t  p, bytes;

  randomSeed(analogRead(A0));              // Randomize w/unused analog pin
  matrix.begin();
  
  for(i=0; i<256; i++) { // Initialize gamma-correction table:
    gamma8[i] = (uint8_t)(pow(((float)i / 255.0), GAMMA) * 255.0 + 0.5);
  }
}


// LOOP - RUNS ONCE PER FRAME OF ANIMATION ---------------------------------

uint32_t prevTime  = 0x7FFFFFFF; // Used for frame-to-frame animation timing
uint32_t frameUsec = 0L;         // Frame interval in microseconds

void loop() {
  matrix.setRotation(0);
  // Wait for FPS interval to elapse (this approach is more consistent than
  // delay() as the animation rendering itself takes indeterminate time).
  uint32_t t;
  while(((t = micros()) - prevTime) < frameUsec);
  prevTime = t;

  // Display frame rendered on prior pass.  This is done immediately
  // after the FPS sync (rather than after rendering) to ensure more
  // uniform animation timing.
  displayAndFlipPage();
  //matrix.displayFrame(page);
  //page ^= 1;           // Flip front/back buffer index

  anim[seq[idx]].loop();                     // Render next frame
  frameUsec = 1000000L / anim[seq[idx]].fps; // Frame hold time

  // Write img[] array to matrix thru gamma correction table
  //uint8_t i, bytes; // Pixel #, Wire buffer counter
  //matrix.setFrame(page);
  //matrix.clear();

  copyImg();

  // Time for new mode?
  if((t - modeStartTime) > (anim[seq[idx]].maxRunTime * 1000000L)) {
    if(++idx >= sizeof(seq)) idx = 0;
    clearImg();
    //memset(img, 0, sizeof(img));
    if(anim[seq[idx]].setup) anim[seq[idx]].setup();
    modeStartTime = t;
    frame = 0;
  } 
  else 
    frame++;
}


/********************************
 * Utilities
 ********************************/

void clearImg() {
    memset(img, 0, sizeof(img));
}

void copyImg() {
  for (int16_t x = 0; x<WIDTH; x++) {
    for (int16_t y = 0; y<HEIGHT; y++) {
      matrix.drawPixel(x,y,gamma8[img[y*WIDTH+x]]);
    }
  }
}

void displayAndFlipPage() {
  // Display the previous built page.
  matrix.displayFrame(page);
  // Flip to new page
  page ^= 1;           // Flip front/back buffer index
  matrix.setFrame(page);
  // And clear the contents for the next page.
  matrix.clear();
}
