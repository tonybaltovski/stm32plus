/*
 * This file is a part of the open source stm32plus library.
 * Copyright (c) 2011,2012,2013,2014 Andy Brown <www.andybrown.me.uk>
 * Please see website for licensing terms.
 */

#pragma once


namespace stm32plus {
  namespace display {


    /**
     * Specialisation for 48Mhz HCLK, 64K colour mode, 42ns low and 42ns high write cycle periods.
     * The 20.8ns HCLK means that we actually achieve 41.6ns (+/- clock accuracy).
     * This driver is intended for use with the 48MHz products, for example the F0 Discovery board.
     */

    template<class TPinPackage>
    class Gpio16BitAccessMode<TPinPackage,COLOURS_16BIT,48,42,42> {

      protected:
        uint32_t _controlSetAddress;
        uint32_t _controlResetAddress;
        uint32_t _portOutputRegister;
        uint16_t _wr;
        uint16_t _rs;
        uint32_t _jump;

      public:
        Gpio16BitAccessMode();

        void reset() const;

        void writeCommand(uint16_t command) const;
        void writeCommand(uint16_t command,uint16_t parameter) const;
        void writeData(uint16_t value) const;
        void writeDataAgain(uint16_t value) const;
        void writeMultiData(uint32_t howMuch,uint16_t value) const;

        void rawTransfer(const void *buffer,uint32_t numWords) const;
    };


    /**
     * Constructor
     */

    template<class TPinPackage>
    inline Gpio16BitAccessMode<TPinPackage,COLOURS_16BIT,48,42,42>::Gpio16BitAccessMode() {

      // the assembly code needs these

      _rs=TPinPackage::Pin_RS;
      _wr=TPinPackage::Pin_WR;

      // these are the addresses of the reset/set registers in the normal peripheral region.

#if defined(STM32PLUS_F1) || defined(STM32PLUS_F0)
      _controlResetAddress=TPinPackage::Port_CONTROL+offsetof(GPIO_TypeDef,BRR);
      _controlSetAddress=TPinPackage::Port_CONTROL+offsetof(GPIO_TypeDef,BSRR);
#elif defined(STM32PLUS_F4)
      _controlResetAddress=TPinPackage::Port_CONTROL+offsetof(GPIO_TypeDef,BSRRH);
      _controlSetAddress=TPinPackage::Port_CONTROL+offsetof(GPIO_TypeDef,BSRRL);
#else
#error Unsupported MCU
#endif

      // this is the address of the data output ODR register in the normal peripheral region.

      _portOutputRegister=TPinPackage::Port_DATA+offsetof(GPIO_TypeDef,ODR);

      // all 16 port pins to output, 50MHz slew rate

      GpioPinInitialiser::initialise((GPIO_TypeDef *)TPinPackage::Port_DATA,
                                     0xffff,
                                     Gpio::OUTPUT);

      // control pins to output

      GpioPinInitialiser::initialise((GPIO_TypeDef *)TPinPackage::Port_CONTROL,
                                     TPinPackage::Pin_RS | TPinPackage::Pin_WR | TPinPackage::Pin_RESET,
                                     Gpio::OUTPUT);

      // WR must start as HIGH

      GPIO_SetBits((GPIO_TypeDef *)TPinPackage::Port_CONTROL,TPinPackage::Pin_WR);
    }


    /**
     * Hard-reset the panel
     */

    template<class TPinPackage>
    inline void Gpio16BitAccessMode<TPinPackage,COLOURS_16BIT,48,42,42>::reset() const {

      GPIO_TypeDef *port;

      // let the power stabilise

      MillisecondTimer::delay(10);

      // reset sequence

      port=(GPIO_TypeDef *)TPinPackage::Port_CONTROL;

      GPIO_SetBits(port,TPinPackage::Pin_RESET);
      MillisecondTimer::delay(5);
      GPIO_ResetBits(port,TPinPackage::Pin_RESET);
      MillisecondTimer::delay(50);
      GPIO_SetBits(port,TPinPackage::Pin_RESET);
      MillisecondTimer::delay(50);
    }


    /**
     * Write a command
     * @param command The command register
     */

    template<class TPinPackage>
    inline void Gpio16BitAccessMode<TPinPackage,COLOURS_16BIT,48,42,42>::writeCommand(uint16_t command) const {

      __asm volatile(
        " str  %[value], [%[data]]          \n\t"     // port <= value
        " str  %[rs],    [%[creset], #0]    \n\t"     // [rs] = 0
        " str  %[wr],    [%[creset], #0]    \n\t"     // [wr] = 0
        " str  %[wr],    [%[cset], #0]      \n\t"     // [wr] = 1
        :: [creset]   "l" (_controlResetAddress),     // the control reset address
           [cset]     "l" (_controlSetAddress),       // the control set address
           [data]     "l" (_portOutputRegister),      // the data port
           [wr]       "l" (_wr),                      // WR pin bit
           [rs]       "l" (_rs),                      // RS pin bit
           [value]    "l" (command)                   // input value
      );
    }


    /**
     * Write a command and its parameter (convenience function)
     * @param command The command register
     * @param parameter The register parameter
     */

    template<class TPinPackage>
    inline void Gpio16BitAccessMode<TPinPackage,COLOURS_16BIT,48,42,42>::writeCommand(uint16_t command,uint16_t parameter) const {
      writeCommand(command);
      writeData(parameter);
    }


    /**
     * Write a data value
     * @param value The data value to write
     */

    template<class TPinPackage>
    inline void Gpio16BitAccessMode<TPinPackage,COLOURS_16BIT,48,42,42>::writeData(uint16_t value) const {

      __asm volatile(
        " str  %[value], [%[data]]          \n\t"     // port <= value
        " str  %[rs],    [%[cset], #0]      \n\t"     // [rs] = 1
        " str  %[wr],    [%[creset], #0]    \n\t"     // [wr] = 0
        " str  %[wr],    [%[cset], #0]      \n\t"     // [wr] = 1
        :: [creset]   "l" (_controlResetAddress),     // the control reset address
           [cset]     "l" (_controlSetAddress),       // the control set address
           [data]     "l" (_portOutputRegister),      // the data port
           [wr]       "l" (TPinPackage::Pin_WR),      // WR pin bit
           [rs]       "l" (TPinPackage::Pin_RS),      // RS pin bit
           [value]    "l" (value)                     // input value
      );
    }


    /**
     * Write the same data value that we have recently written out. This is one of our optimisation
     * points. We don't have to do the whole 8080 transaction again and can just toggle WR.
     * @param value The data value to write
     */

    template<class TPinPackage>
    inline void Gpio16BitAccessMode<TPinPackage,COLOURS_16BIT,48,42,42>::writeDataAgain(uint16_t /* value */) const {

      __asm volatile(
        " str  %[wr], [%[creset], #0]   \n\t"       // [wr] = 0
        " str  %[wr], [%[cset], #0]     \n\t"       // [wr] = 1
        :: [creset]   "l" (_controlResetAddress),   // the control reset address
           [cset]     "l" (_controlSetAddress),     // the control set address
           [wr]       "l" (TPinPackage::Pin_WR)     // WR pin bit
      );
    }


    /**
     * Write a batch of the same data values to the XMEM interface using GPIO. The values are written out in a
     * highly optimised loop in bursts of 40 at a time. This value seems a good trade off between flash usage
     * and speed. Note the use of %= labels so that inlining doesn't produce duplicate names.
     * @param howMuch The number of 16-bit values to write
     * @param value The data value to write
     */

    template<class TPinPackage>
    void Gpio16BitAccessMode<TPinPackage,COLOURS_16BIT,48,42,42>::writeMultiData(uint32_t howMuch,uint16_t value) const {

      // F0 compatibility : value, data, rs are only needed at the start so move
      // them to own asm section so gcc doesn't have to find so many registers
      // in the next block

      __asm volatile(
        "str  %[value], [%[data]]                       \n\t"     // port <= value
        "str  %[rs],    [%[cset], #0]                   \n\t"     // [rs] = 1
        :: [rs]       "l" (TPinPackage::Pin_RS),      // RS pin bit
           [value]    "l" (value),                    // input value
           [cset]     "l" (_controlSetAddress),       // the control set address
           [data]     "l" (_portOutputRegister)       // the data port
      );

      // this is the main block

      __asm volatile(
          "    cmp  %[howmuch], #40                       \n\t"     // if less than 40 then go straight
          "    blo  lastlot%=                             \n\t"     // to the finishing off stage

          // the following loop shows the fastest that you can toggle a GPIO pin
          // on the STM32.

          "batchloop%=:                                   \n\t"
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1

          "    sub  %[howmuch], #40                       \n\t"     // subtract 40 from howMuch
          "    cmp  %[howmuch], #40                       \n\t"     // if howMuch >= 40 then go back for another batch
          "    bhs  batchloop%=                           \n\t"

          "lastlot%=:                                     \n\t"
          "    ldr %[jump],    =finished%=                \n\t"     // load 'jump' with the address of the end
          "    lsl %[howmuch], #2                         \n\t"     // multiply remaining by 4 and
          "    sub %[jump],    %[howmuch]                 \n\t"     // subtract from the 'jump' target
          "    add %[jump],    #1                         \n\t"     // set thumb mode (bit 0=1)
          "    bx  %[jump]                                \n\t"     // indirect jump forward into the last lot

          // there are 39 writes here

          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1
          "    str  %[wr], [%[creset], #0]                \n\t"     // [wr] = 0
          "    str  %[wr], [%[cset], #0]                  \n\t"     // [wr] = 1

          "finished%=:                                    \n\t"

          :: [creset]   "l" (_controlResetAddress),     // the control reset address
             [cset]     "l" (_controlSetAddress),       // the control set address
             [wr]       "l" (TPinPackage::Pin_WR),      // WR pin bit
             [jump]     "l" (_jump),                    // holds calculated indirect jump target
             [howmuch]  "l" (howMuch)                   // number of pixels to write
      );
    }


    /**
     * Write out a raw block of data from memory
     * @param buffer Where to read from
     * @param numWords The number of 16-bit words
     */

    template<class TPinPackage>
    inline void Gpio16BitAccessMode<TPinPackage,COLOURS_16BIT,48,42,42>::rawTransfer(const void *buffer,uint32_t numWords) const {

      const uint16_t *ptr=static_cast<const uint16_t *>(buffer);

      while(numWords--)
        writeData(*ptr++);
    }
  }
}
