/**
 ********************************************************************************
 * @file    Libs_Ssd1306.h
 * @author  tuann
 * @date    Jan 26, 2025
 * @brief
 ********************************************************************************
 */

#ifndef LIBS_SSD1306_H_
#define LIBS_SSD1306_H_

#ifdef __cplusplus
extern "C" {
#endif

/************************************
 * INCLUDES
 ************************************/
#include <stdint.h>
/************************************
 * MACROS AND DEFINES
 ************************************/
#define SSD1306_I2C_ADDR                        0x3C

#define SSD1306_X_OFFSET_LOWER                  0x00
#define SSD1306_X_OFFSET_UPPER                  0x00

#define SSD1306_HEIGHT                          0x40
#define SSD1306_WIDTH                           0x80

#define SSD1306_BUFFER_SIZE                     SSD1306_WIDTH * SSD1306_HEIGHT / 8

#define SSD1306_INCLUDE_FONT_6x8
// #define SSD1306_INCLUDE_FONT_7x10
// #define SSD1306_INCLUDE_FONT_11x18
// #define SSD1306_INCLUDE_FONT_16x26

/************************************
 * TYPEDEFS
 ************************************/
typedef enum {
	SSD1306_BLACK = 0x00, // Black color, no pixel
	SSD1306_WHITE = 0x01  // Pixel is set. Color depends on OLED
} SSD1306_COLOR;

typedef enum {
	SSD1306_OK = 0x00, SSD1306_ERR = 0x01  // Generic error.
} SSD1306_Error_t;

// Struct to store transformations
typedef struct {
	uint16_t CurrentX;
	uint16_t CurrentY;
	uint8_t Initialized;
	uint8_t DisplayOn;
} SSD1306_t;

typedef struct {
	uint8_t x;
	uint8_t y;
} SSD1306_VERTEX;

typedef struct {
	const uint8_t FontWidth; /*!< Font width in pixels */
	uint8_t FontHeight; /*!< Font height in pixels */
	const uint16_t *data; /*!< Pointer to data font data array */
} FontDef;

/************************************
 * EXPORTED VARIABLES
 ************************************/
#ifdef SSD1306_INCLUDE_FONT_6x8
extern FontDef Font_6x8;
#endif
#ifdef SSD1306_INCLUDE_FONT_7x10
extern FontDef Font_7x10;
#endif
#ifdef SSD1306_INCLUDE_FONT_11x18
extern FontDef Font_11x18;
#endif
#ifdef SSD1306_INCLUDE_FONT_16x26
extern FontDef Font_16x26;
#endif
#ifdef SSD1306_INCLUDE_FONT_16x24
extern FontDef Font_16x24;
#endif

/************************************
 * GLOBAL FUNCTION PROTOTYPES
 ************************************/
void Libs_Ssd1306_Init();
void Libs_Ssd1306_Fill(SSD1306_COLOR color);
void Libs_Ssd1306_UpdateScreen();
void Libs_Ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color);
char Libs_Ssd1306_WriteChar(char ch, FontDef Font, SSD1306_COLOR color);
char Libs_Ssd1306_WriteString(char *str, FontDef Font, SSD1306_COLOR color);
void Libs_Ssd1306_SetCursor(uint8_t x, uint8_t y);
void Libs_Ssd1306_Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
		SSD1306_COLOR color);
void Libs_Ssd1306_DrawArc(uint8_t x, uint8_t y, uint8_t radius,
		uint16_t start_angle, uint16_t sweep, SSD1306_COLOR color);
void Libs_Ssd1306_DrawArcWithRadiusLine(uint8_t x, uint8_t y, uint8_t radius,
		uint16_t start_angle, uint16_t sweep, SSD1306_COLOR color);
void Libs_Ssd1306_DrawCircle(uint8_t par_x, uint8_t par_y, uint8_t par_r,
		SSD1306_COLOR color);
void Libs_Ssd1306_FillCircle(uint8_t par_x, uint8_t par_y, uint8_t par_r,
		SSD1306_COLOR par_color);
void Libs_Ssd1306_Polyline(const SSD1306_VERTEX *par_vertex, uint16_t par_size,
		SSD1306_COLOR color);
void Libs_Ssd1306_DrawRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
		SSD1306_COLOR color);
void Libs_Ssd1306_FillRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
		SSD1306_COLOR color);
void Libs_Ssd1306_DrawBitmap(uint8_t x, uint8_t y, const unsigned char *bitmap,
		uint8_t w, uint8_t h, SSD1306_COLOR color);

void Libs_Ssd1306_SetContrast(const uint8_t value);
void Libs_Ssd1306_SetDisplayOn(const uint8_t on);
uint8_t Libs_Ssd1306_GetDisplayOn();

void Libs_Ssd1306_WriteCommand(uint8_t byte);
void Libs_Ssd1306_WriteData(uint8_t *buffer, uint32_t buff_size);
SSD1306_Error_t Libs_Ssd1306_FillBuffer(uint8_t *buf, uint32_t len);

/* Test function */
void Libs_Ssd1306_TestDrawBitmap();

#ifdef __cplusplus
}
#endif

#endif /* LIBS_SSD1306_H_ */
