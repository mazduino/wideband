#include "ch.h"
#include "hal.h"

#include "wideband_config.h"

#if HAL_USE_I2C && defined(OLED_I2C_DRIVER)

#include "oled.h"

#include "fault.h"
#include "heater_control.h"
#include "lambda_conversion.h"
#include "sampling.h"

#include <cstring>

using namespace wbo;

// *******************************
//    Panel geometry
// *******************************

#if (OLED_HEIGHT % 8) != 0
#error "OLED_HEIGHT must be a multiple of 8"
#endif

#define OLED_PAGES      (OLED_HEIGHT / 8)
#define OLED_FB_SIZE    (OLED_WIDTH * OLED_PAGES)

// SH1106 has 132 columns of RAM but only 128 are visible, centered
#ifdef OLED_CONTROLLER_SH1106
    #define OLED_COL_OFFSET 2
#else
    #define OLED_COL_OFFSET 0
#endif

// One channel block is 32px tall, so a 128x64 panel shows two channels
#define OLED_BLOCK_HEIGHT   32
#define OLED_BLOCKS         (OLED_HEIGHT / OLED_BLOCK_HEIGHT)

// Control bytes prefixing every I2C payload
#define OLED_CTRL_COMMAND   0x00
#define OLED_CTRL_DATA      0x40

// *******************************
//    5x7 font, ASCII 0x20..0x7E
// *******************************
// Column-major, bit 0 is the top row. Taken from the Adafruit GFX "glcdfont"
// 5x7 table (BSD licensed), trimmed to the printable ASCII range.

#define FONT_FIRST_CHAR 0x20
#define FONT_LAST_CHAR  0x7E
#define FONT_WIDTH      5
#define FONT_HEIGHT     7
// One column of spacing between characters
#define FONT_ADVANCE    (FONT_WIDTH + 1)

static const uint8_t FontData[] = {
    0x00, 0x00, 0x00, 0x00, 0x00,  // ' '
    0x00, 0x00, 0x5F, 0x00, 0x00,  // '!'
    0x00, 0x07, 0x00, 0x07, 0x00,  // '"'
    0x14, 0x7F, 0x14, 0x7F, 0x14,  // '#'
    0x24, 0x2A, 0x7F, 0x2A, 0x12,  // '$'
    0x23, 0x13, 0x08, 0x64, 0x62,  // '%'
    0x36, 0x49, 0x56, 0x20, 0x50,  // '&'
    0x00, 0x08, 0x07, 0x03, 0x00,  // '''
    0x00, 0x1C, 0x22, 0x41, 0x00,  // '('
    0x00, 0x41, 0x22, 0x1C, 0x00,  // ')'
    0x2A, 0x1C, 0x7F, 0x1C, 0x2A,  // '*'
    0x08, 0x08, 0x3E, 0x08, 0x08,  // '+'
    0x00, 0x80, 0x70, 0x30, 0x00,  // ','
    0x08, 0x08, 0x08, 0x08, 0x08,  // '-'
    0x00, 0x00, 0x60, 0x60, 0x00,  // '.'
    0x20, 0x10, 0x08, 0x04, 0x02,  // '/'
    0x3E, 0x51, 0x49, 0x45, 0x3E,  // '0'
    0x00, 0x42, 0x7F, 0x40, 0x00,  // '1'
    0x72, 0x49, 0x49, 0x49, 0x46,  // '2'
    0x21, 0x41, 0x49, 0x4D, 0x33,  // '3'
    0x18, 0x14, 0x12, 0x7F, 0x10,  // '4'
    0x27, 0x45, 0x45, 0x45, 0x39,  // '5'
    0x3C, 0x4A, 0x49, 0x49, 0x31,  // '6'
    0x41, 0x21, 0x11, 0x09, 0x07,  // '7'
    0x36, 0x49, 0x49, 0x49, 0x36,  // '8'
    0x46, 0x49, 0x49, 0x29, 0x1E,  // '9'
    0x00, 0x00, 0x14, 0x00, 0x00,  // ':'
    0x00, 0x40, 0x34, 0x00, 0x00,  // ';'
    0x00, 0x08, 0x14, 0x22, 0x41,  // '<'
    0x14, 0x14, 0x14, 0x14, 0x14,  // '='
    0x00, 0x41, 0x22, 0x14, 0x08,  // '>'
    0x02, 0x01, 0x59, 0x09, 0x06,  // '?'
    0x3E, 0x41, 0x5D, 0x59, 0x4E,  // '@'
    0x7C, 0x12, 0x11, 0x12, 0x7C,  // 'A'
    0x7F, 0x49, 0x49, 0x49, 0x36,  // 'B'
    0x3E, 0x41, 0x41, 0x41, 0x22,  // 'C'
    0x7F, 0x41, 0x41, 0x41, 0x3E,  // 'D'
    0x7F, 0x49, 0x49, 0x49, 0x41,  // 'E'
    0x7F, 0x09, 0x09, 0x09, 0x01,  // 'F'
    0x3E, 0x41, 0x41, 0x51, 0x73,  // 'G'
    0x7F, 0x08, 0x08, 0x08, 0x7F,  // 'H'
    0x00, 0x41, 0x7F, 0x41, 0x00,  // 'I'
    0x20, 0x40, 0x41, 0x3F, 0x01,  // 'J'
    0x7F, 0x08, 0x14, 0x22, 0x41,  // 'K'
    0x7F, 0x40, 0x40, 0x40, 0x40,  // 'L'
    0x7F, 0x02, 0x1C, 0x02, 0x7F,  // 'M'
    0x7F, 0x04, 0x08, 0x10, 0x7F,  // 'N'
    0x3E, 0x41, 0x41, 0x41, 0x3E,  // 'O'
    0x7F, 0x09, 0x09, 0x09, 0x06,  // 'P'
    0x3E, 0x41, 0x51, 0x21, 0x5E,  // 'Q'
    0x7F, 0x09, 0x19, 0x29, 0x46,  // 'R'
    0x26, 0x49, 0x49, 0x49, 0x32,  // 'S'
    0x03, 0x01, 0x7F, 0x01, 0x03,  // 'T'
    0x3F, 0x40, 0x40, 0x40, 0x3F,  // 'U'
    0x1F, 0x20, 0x40, 0x20, 0x1F,  // 'V'
    0x3F, 0x40, 0x38, 0x40, 0x3F,  // 'W'
    0x63, 0x14, 0x08, 0x14, 0x63,  // 'X'
    0x03, 0x04, 0x78, 0x04, 0x03,  // 'Y'
    0x61, 0x59, 0x49, 0x4D, 0x43,  // 'Z'
    0x00, 0x7F, 0x41, 0x41, 0x41,  // '['
    0x02, 0x04, 0x08, 0x10, 0x20,  // '\'
    0x00, 0x41, 0x41, 0x41, 0x7F,  // ']'
    0x04, 0x02, 0x01, 0x02, 0x04,  // '^'
    0x40, 0x40, 0x40, 0x40, 0x40,  // '_'
    0x00, 0x03, 0x07, 0x08, 0x00,  // '`'
    0x20, 0x54, 0x54, 0x78, 0x40,  // 'a'
    0x7F, 0x28, 0x44, 0x44, 0x38,  // 'b'
    0x38, 0x44, 0x44, 0x44, 0x28,  // 'c'
    0x38, 0x44, 0x44, 0x28, 0x7F,  // 'd'
    0x38, 0x54, 0x54, 0x54, 0x18,  // 'e'
    0x00, 0x08, 0x7E, 0x09, 0x02,  // 'f'
    0x18, 0xA4, 0xA4, 0x9C, 0x78,  // 'g'
    0x7F, 0x08, 0x04, 0x04, 0x78,  // 'h'
    0x00, 0x44, 0x7D, 0x40, 0x00,  // 'i'
    0x20, 0x40, 0x40, 0x3D, 0x00,  // 'j'
    0x7F, 0x10, 0x28, 0x44, 0x00,  // 'k'
    0x00, 0x41, 0x7F, 0x40, 0x00,  // 'l'
    0x7C, 0x04, 0x78, 0x04, 0x78,  // 'm'
    0x7C, 0x08, 0x04, 0x04, 0x78,  // 'n'
    0x38, 0x44, 0x44, 0x44, 0x38,  // 'o'
    0xFC, 0x18, 0x24, 0x24, 0x18,  // 'p'
    0x18, 0x24, 0x24, 0x18, 0xFC,  // 'q'
    0x7C, 0x08, 0x04, 0x04, 0x08,  // 'r'
    0x48, 0x54, 0x54, 0x54, 0x24,  // 's'
    0x04, 0x04, 0x3F, 0x44, 0x24,  // 't'
    0x3C, 0x40, 0x40, 0x20, 0x7C,  // 'u'
    0x1C, 0x20, 0x40, 0x20, 0x1C,  // 'v'
    0x3C, 0x40, 0x30, 0x40, 0x3C,  // 'w'
    0x44, 0x28, 0x10, 0x28, 0x44,  // 'x'
    0x4C, 0x90, 0x90, 0x90, 0x7C,  // 'y'
    0x44, 0x64, 0x54, 0x4C, 0x44,  // 'z'
    0x00, 0x08, 0x36, 0x41, 0x00,  // '{'
    0x00, 0x00, 0x77, 0x00, 0x00,  // '|'
    0x00, 0x41, 0x36, 0x08, 0x00,  // '}'
    0x02, 0x01, 0x02, 0x04, 0x02,  // '~'
};

static_assert(sizeof(FontData) == (FONT_LAST_CHAR - FONT_FIRST_CHAR + 1) * FONT_WIDTH,
              "font table size mismatch");

// *******************************
//    Framebuffer
// *******************************

static uint8_t fb[OLED_FB_SIZE];
// Page transfer buffer: control byte followed by one page of pixels
static uint8_t pageBuf[1 + OLED_WIDTH];

static bool displayReady = false;

// *******************************
//    I2C transport
// *******************************

static const I2CConfig i2cCfg = {
    OPMODE_I2C,
    OLED_I2C_BITRATE,
    (OLED_I2C_BITRATE > 100000) ? FAST_DUTY_CYCLE_2 : STD_DUTY_CYCLE,
};

static bool I2cSend(const uint8_t* buf, size_t len)
{
    i2cAcquireBus(OLED_I2C_DRIVER);
    msg_t status = i2cMasterTransmitTimeout(OLED_I2C_DRIVER, OLED_I2C_ADDRESS,
                                            buf, len, nullptr, 0, TIME_MS2I(50));
    i2cReleaseBus(OLED_I2C_DRIVER);

    return status == MSG_OK;
}

static const uint8_t InitSequence[] = {
    OLED_CTRL_COMMAND,
    0xAE,                       // display off
    0xD5, 0x80,                 // clock divide ratio / oscillator frequency
    0xA8, OLED_HEIGHT - 1,      // multiplex ratio
    0xD3, 0x00,                 // display offset
    0x40,                       // start line = 0
#ifdef OLED_CONTROLLER_SH1106
    0xAD, 0x8B,                 // DC-DC converter on
    0x32,                       // pump voltage 8.0V
#else
    0x8D, 0x14,                 // charge pump on
    0x20, 0x02,                 // page addressing mode
#endif
#ifdef OLED_ROTATE_180
    0xA0,                       // segment remap: column 0 -> SEG0
    0xC0,                       // COM scan direction: normal
#else
    0xA1,                       // segment remap: column 0 -> SEG127
    0xC8,                       // COM scan direction: remapped
#endif
#if OLED_HEIGHT >= 64
    0xDA, 0x12,                 // COM pins: alternative, no left/right remap
#else
    0xDA, 0x02,                 // COM pins: sequential, no left/right remap
#endif
    0x81, 0xCF,                 // contrast
    0xD9, 0xF1,                 // pre-charge period
    0xDB, 0x40,                 // VCOMH deselect level
    0xA4,                       // resume from RAM content
    0xA6,                       // non-inverted
    0xAF,                       // display on
};

static bool InitDisplay()
{
    return I2cSend(InitSequence, sizeof(InitSequence));
}

static bool WritePage(uint8_t page)
{
    const uint8_t col = OLED_COL_OFFSET;
    const uint8_t cmds[] = {
        OLED_CTRL_COMMAND,
        (uint8_t)(0xB0 | page),         // page address
        (uint8_t)(col & 0x0F),          // column address, low nibble
        (uint8_t)(0x10 | (col >> 4)),   // column address, high nibble
    };

    if (!I2cSend(cmds, sizeof(cmds))) {
        return false;
    }

    pageBuf[0] = OLED_CTRL_DATA;
    memcpy(&pageBuf[1], &fb[page * OLED_WIDTH], OLED_WIDTH);

    return I2cSend(pageBuf, sizeof(pageBuf));
}

// *******************************
//    Drawing primitives
// *******************************

static void Clear()
{
    memset(fb, 0x00, sizeof(fb));
}

static void SetPixel(int x, int y)
{
    if ((x < 0) || (x >= OLED_WIDTH) || (y < 0) || (y >= OLED_HEIGHT)) {
        return;
    }

    fb[(y / 8) * OLED_WIDTH + x] |= (1 << (y % 8));
}

// Returns the x advance, so calls can be chained
static int DrawChar(int x, int y, char c, int scale)
{
    if ((c < FONT_FIRST_CHAR) || (c > FONT_LAST_CHAR)) {
        c = '?';
    }

    const uint8_t* glyph = &FontData[(c - FONT_FIRST_CHAR) * FONT_WIDTH];

    for (int col = 0; col < FONT_WIDTH; col++) {
        uint8_t bits = glyph[col];

        for (int row = 0; row < FONT_HEIGHT; row++) {
            if ((bits & (1 << row)) == 0) {
                continue;
            }

            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    SetPixel(x + col * scale + dx, y + row * scale + dy);
                }
            }
        }
    }

    return FONT_ADVANCE * scale;
}

static int DrawString(int x, int y, const char* str, int scale)
{
    while (*str) {
        x += DrawChar(x, y, *str++, scale);
    }

    return x;
}

static int StringWidth(const char* str, int scale)
{
    return strlen(str) * FONT_ADVANCE * scale;
}

// *******************************
//    Number formatting
// *******************************
// chprintf is built without float support (CHPRINTF_USE_FLOAT), so format
// fixed point values by hand instead of pulling in a softfloat printf.

static char* PutUInt(char* p, const char* end, uint32_t value, int minDigits)
{
    char tmp[10];
    int n = 0;

    do {
        tmp[n++] = '0' + (value % 10);
        value /= 10;
    } while ((value != 0) && (n < (int)sizeof(tmp)));

    while ((n < minDigits) && (n < (int)sizeof(tmp))) {
        tmp[n++] = '0';
    }

    while ((n > 0) && (p < end)) {
        *p++ = tmp[--n];
    }

    return p;
}

static void FormatFixed(char* buf, size_t size, float value, int decimals)
{
    char* p = buf;
    const char* end = buf + size - 1;

    // Also catches NaN, since every comparison against NaN is false
    if (!((value > -100000.0f) && (value < 100000.0f))) {
        strncpy(buf, "---", size - 1);
        buf[size - 1] = '\0';
        return;
    }

    if (value < 0) {
        if (p < end) {
            *p++ = '-';
        }
        value = -value;
    }

    uint32_t mul = 1;
    for (int i = 0; i < decimals; i++) {
        mul *= 10;
    }

    uint32_t scaled = (uint32_t)(value * mul + 0.5f);

    p = PutUInt(p, end, scaled / mul, 1);

    if (decimals > 0) {
        if (p < end) {
            *p++ = '.';
        }
        p = PutUInt(p, end, scaled % mul, decimals);
    }

    *p = '\0';
}

// *******************************
//    Screen layout
// *******************************

static const char* DescribeFault(Fault fault)
{
    switch (fault) {
        case Fault::None:
            return "";
        case Fault::SensorDidntHeat:
            return "NO HEAT";
        case Fault::SensorOverheat:
            return "OVERHEAT";
        case Fault::SensorUnderheat:
            return "UNDERHEAT";
        case Fault::SensorNoHeatSupply:
            return "NO SUPPLY";
    }

    return "FAULT";
}

static void Render()
{
    Clear();

    for (int ch = 0; (ch < AFR_CHANNELS) && (ch < OLED_BLOCKS); ch++) {
        const int base = ch * OLED_BLOCK_HEIGHT;
        const ISampler& sampler = GetSampler(ch);
        char buf[10];

#if AFR_CHANNELS > 1
        // Channel label
        DrawChar(0, base, (ch == 0) ? 'L' : 'R', 1);
#endif

        // Heater supply voltage, top right corner. Only channel 0 senses it.
        if (ch == 0) {
            FormatFixed(buf, sizeof(buf), sampler.GetInternalHeaterVoltage(), 1);
            int x = OLED_WIDTH - StringWidth(buf, 1) - FONT_ADVANCE;
            x = DrawString(x, base, buf, 1);
            DrawString(x, base, "V", 1);
        }

        Fault fault = GetCurrentFault(ch);
        if (fault != Fault::None) {
            DrawString(0, base + 12, DescribeFault(fault), 2);
            continue;
        }

        float lambda = GetLambda(ch);

        // Big AFR reading, 5 characters at double size = 60px wide
        FormatFixed(buf, sizeof(buf), lambda * OLED_AFR_STOICH, 2);
        DrawString(0, base + 10, buf, 2);

        // Lambda, to the right of the AFR
        int x = DrawString(66, base + 10, "La", 1);
        FormatFixed(buf, sizeof(buf), lambda, 3);
        DrawString(x + 3, base + 10, buf, 1);

        // Sensor temperature and heater duty below it
        FormatFixed(buf, sizeof(buf), sampler.GetSensorTemperature(), 0);
        x = DrawString(66, base + 20, buf, 1);
        x = DrawString(x, base + 20, "C ", 1);
        FormatFixed(buf, sizeof(buf), GetHeaterDuty(ch) * 100.0f, 0);
        x = DrawString(x, base + 20, buf, 1);
        DrawString(x, base + 20, "%", 1);
    }
}

// *******************************
//    Display thread
// *******************************

static THD_WORKING_AREA(waOledThread, 512);

static void OledThread(void*)
{
    chRegSetThreadName("OLED");

    while (true) {
        if (!displayReady) {
            displayReady = InitDisplay();

            if (!displayReady) {
                // Nothing answered - the panel may be plugged in later, keep trying
                chThdSleepMilliseconds(1000);
                continue;
            }
        }

        Render();

        for (uint8_t page = 0; page < OLED_PAGES; page++) {
            if (!WritePage(page)) {
                // Bus error or the display went away: bounce the driver so a
                // stuck bus gets released, then re-run the init sequence.
                displayReady = false;
                i2cStop(OLED_I2C_DRIVER);
                i2cStart(OLED_I2C_DRIVER, &i2cCfg);
                break;
            }
        }

        chThdSleepMilliseconds(OLED_REFRESH_PERIOD_MS);
    }
}

void InitOled()
{
    i2cStart(OLED_I2C_DRIVER, &i2cCfg);

    chThdCreateStatic(waOledThread, sizeof(waOledThread), NORMALPRIO - 1, OledThread, nullptr);
}

#else /* HAL_USE_I2C && defined(OLED_I2C_DRIVER) */

void InitOled()
{
}

#endif
