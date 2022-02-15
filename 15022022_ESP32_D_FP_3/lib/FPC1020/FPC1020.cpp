#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include "FPC1020.h"

//#define MISO   19
//#define MOSI   23
//#define SCLK   18
//#define SS     5
//#define FPC_IRQ 12
//#define FPC_RST 14

#define MISO   12
#define MOSI   13
#define SCLK   14
#define SS     15
#define FPC_IRQ 4 //18 //32
#define FPC_RST 16 //5 //33
// #define FPC_IRQ 32 //32
// #define FPC_RST 33 //33


static const int spiClk = 16000000; // 1 MHz
uint8_t rBuf[36864];


SPIClass * vspi = NULL;

void FPC1020::init()
{
    vspi = new SPIClass(VSPI);
    // Chip select
    pinMode(SS, OUTPUT);
    digitalWrite(SS, HIGH);

    // IRQ / data ready
    pinMode(FPC_IRQ, INPUT);
    digitalWrite(FPC_IRQ, LOW);

    // RST
    pinMode(FPC_RST, OUTPUT);

    vspi->begin(SCLK, MISO, MOSI, SS);
    vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
}

void FPC1020::reset()
{
    digitalWrite(FPC_RST, LOW);
    delay(10);
    digitalWrite(FPC_RST, HIGH);
}

void FPC1020::setup()
{
    setup_rev3();
}

void FPC1020::setup_rev3()
{
  transmit16(FPC102X_REG_ADC_SHIFT_GAIN, 0x0a02);
  transmit16(FPC102X_REG_PXL_CTRL, 0x0f1e); 
}

uint8_t FPC1020::interrupt(bool clear)
{
    return transmit8(clear ? FPC102X_REG_READ_INTERRUPT_WITH_CLEAR : FPC102X_REG_READ_INTERRUPT, 0);
}

uint8_t FPC1020::error()
{
    return transmit8(FPC102X_REG_READ_ERROR_WITH_CLEAR, 0);
}

uint16_t FPC1020::hardware_id()
{
    return transmit16(FPC102X_REG_HWID, 0);
}

uint16_t FPC1020::finger_present_status()
{
    return transmit16(FPC102X_REG_FINGER_PRESENT_STATUS, 0);
}

uint64_t FPC1020::fpc1020_read_image()
{
    return transmit64(FPC1020X_REG_READ_IMAGE_DATA, 0);
}

void FPC1020::capture_image()
{  
    digitalWrite(FPC_IRQ, LOW);
    digitalWrite(SS, LOW);
    vspi->write(0xC4);
    vspi->write(0);
    for(size_t i=0; i < 36864; i++)
    {
       rBuf[i] = vspi->transfer(0);
    }
    digitalWrite(SS, HIGH);
}


void FPC1020::command(fpc1020_reg reg)
{
    digitalWrite(SS, LOW);
    vspi->write(reg);
    digitalWrite(SS, HIGH);
}


uint8_t FPC1020::transmit8(fpc1020_reg reg, uint8_t val)
{
    digitalWrite(SS, LOW);
    vspi->write(reg);
    uint8_t ret = vspi->transfer(val);
    digitalWrite(SS, HIGH);
    return ret;
}

uint16_t FPC1020::transmit16(fpc1020_reg reg, uint16_t val)
{
    digitalWrite(SS, LOW);
    vspi->write(reg);
    uint16_t ret = vspi->transfer16(val);
    digitalWrite(SS, HIGH);
    return ret;
}

uint32_t FPC1020::transmit32(fpc1020_reg reg, uint32_t val)
{
    digitalWrite(SS, LOW);

    vspi->write(reg);

    uint32_t out = 0;
    uint8_t *pout = (uint8_t *)&out;
    uint8_t *pin = (uint8_t *)&val;

    for (unsigned int i = 0; i < sizeof(uint32_t); i++)
    {
        pout[sizeof(uint32_t) - i - 1] = vspi->transfer(pin[sizeof(uint32_t) - i - 1]);
        // pout[i] = vspi->transfer(pin[i]);
    }

    digitalWrite(SS, HIGH);

    return out;
}

uint64_t FPC1020::transmit64(fpc1020_reg reg, uint64_t val)
{
    digitalWrite(SS, LOW);

    vspi->write(reg);

    uint64_t out = 0;
    uint8_t *pout = (uint8_t *)&out;
    uint8_t *pin = (uint8_t *)&val;

    for (unsigned int i = 0; i < sizeof(uint64_t); i++)
    {
        pout[sizeof(uint64_t) - i - 1] = vspi->transfer(pin[sizeof(uint64_t) - i - 1]);
        // pout[i] = vspi->transfer(pin[i]);
    }

    digitalWrite(SS, HIGH);

    return out;
}
