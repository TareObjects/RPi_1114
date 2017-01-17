//  RPi1114 firmware
//
//  TareObjects - koichi kurahashi
//
//  ver 1.00 2017-01-16 first version
//      1.01 2017-01-17 fix bugs : led on when analog read command
//                                 bad analog channel boudary
//                                 
//



#include "mbed.h"



const unsigned short version = 0x0101; //  ver 1.01

const int   kMaxInteger      = 32767;
const float fFloatScaleValue = 1024.0;  // factor to convert long <-> float

//
//  common definition
//

#define kCommandAddress 0x50


//  internal gpio map
//
//  dp5  : SDA - RPi's SDA / pad
//  dp14 : on board LED
//  dp15 : rx - RPi's TXD
//  dp16 : tx - RPi's RXD
//  dp17 : out - RPi's power switch
//  dp23 : reset - RPi's GPIO24
//  dp24 : isp - RPI's GPIO23
//  dp25 : in - sw1 / pad
//  dp27 : SCL - RPi's SCL / pad



//  pad map
//
//  dp1  digital Pwm        : Pwm
//  dp2  digital Pwm        : Pwm
//  dp4  digital analog     : digital in
//  dp5  I2C(SDA)           : RPi(master)'s SDA / pad
//  dp6  digital            : digital in
//  dp9  digital analog     : analog
//  dp10 digital analog     : analog
//  dp11 digital analog     : analog
//  dp13 digital analog
//  do18 digital Pwm        : Pwm
//  dp25 digital in         : sw1 / pad
//  dp26 digital            : digital out
//  dp27 I2C(SCL)           : RPi(master)'s SCL / pad
//  dp28 digital            : digital out




//
//  I2C typical command format
//
//  [0] = command
//  [1] = target ch
//  [2] = format
//        0 : binary
//            [3...] : long
//        1 : ascii string
//            [3...] : c string
//
//  endian : little endian
//

#define kCommandVersion     0x00    //              : get version number ver 1.01

#define kCommandUp          0x10    //  i2c command : RPi power on
#define kCommandDown        0x11    //              : RPi power off

#define kCommandLED         0x20    //              : led - on/off/flash

#define kCommandAnalogRead  0x30    //              : read analog port
#define kCommandAnalogReset 0x31    //              : reset read buffer
#define kCommandAnalogStart 0x32    //              : start fill buffer       
#define kCommandAnalogStop  0x33    //              : stop  fill buffer
#define kCommandAnalogLoad  0x34    //              : load analog data

#define kCommandPwm         0x40    //              : output Pwm
#define kCommandPwmPeriod   0x41    //              : set Pwm period in uSec

#define kCommandDigitalIn   0x50    //              : input  digital

#define kCommandDigitalOut  0x60    //              : output digital



#define kCommandModeBinary  0x00    //  binary mode
#define kCommandModeASCII   0x10    //  ascii  mode    

//
//  I2C
//

#define MaxReceiveBufferSize 100    //  i2c receiving buffer length

I2CSlave slave(dp5, dp27);



//
//  RPi Power Controll
//
//  [0] command
//      turn on : kCommandUp
//      turn off : kCommandDown
//  [1] ch
//      always 0
//  [2] format
//      0 : binary mode
//      1 : c string mode
//  [3...] data
//      binary mode : little endian long
//      ascii mode  : c string
//

DigitalOut rpiPower(dp17);
DigitalOut led(dp14);

int powerMode = kCommandUp;

void powerOff()
{
    rpiPower  = 0;
    powerMode = kCommandDown;
}

void powerOn()
{
    rpiPower  = 1;
    powerMode = kCommandUp;
}



//  led modes
//
//  [0] command
//      led comtrol
//  [1] ch
//      always 0
//  [2] mode
//      0 : turn off
//      1 : turn on
//      2 : flash
//

#define kLED_Off    0   // LED off
#define kLED_On     1   // LED on
#define kLED_Flash  2   // LED flash 

int ledMode   = kLED_Flash;


void ledOn()
{
    led     = 1;
    ledMode = kLED_On;
}

void ledOff()
{
    led     = 0;
    ledMode = kLED_Off;
}




//
//  on board Switch
//
DigitalIn sw1(dp25);
const int SW_OFF = 1;
const int SW_ON  = 0;   // pull up

int sw1Mode   = SW_OFF;
int waitForSw1Release = 0;


//
//  ADC
//

#define kMaxAnalogChannels      3       //  analog read channels .... never change
#define kMinAnalogPeriod    10000       //  10mSec is minimum
#define kMaxAnalogPeriod 10000000       //   10Sec is maximum

const int kMaxAnalogBufferSize  = 256;  //  analog read buffer size

typedef struct {
    unsigned short count;
    unsigned short buffer[kMaxAnalogBufferSize];
} AnalogBuffer;

AnalogBuffer analogBuffer[kMaxAnalogChannels];

AnalogIn analogIn[kMaxAnalogChannels] = {dp9, dp10, dp11};
Ticker analogTicker[kMaxAnalogChannels];
//Ticker t1, t2, t0;


void analogTickerVectors(int ch) {
    if (ch >= 0 && ch < kMaxAnalogChannels) {
        AnalogBuffer *p = &analogBuffer[ch];
        unsigned short count = p->count;
        if (count < (kMaxAnalogBufferSize-1)) {
            float f = analogIn[ch];
            unsigned short v = f * fFloatScaleValue;
            p->buffer[count++] = v;
            p->count = count;
        }
    }
}



void analogTickerVector0()
{
    analogTickerVectors(0);
}

void analogTickerVector1()
{
    analogTickerVectors(1);
}

void analogTickerVector2()
{
    analogTickerVectors(2);
}



float execAnalogIn(int ch)
{
    if (ch >= 0 && ch < kMaxAnalogChannels) {   // fix ver 1.01
        return analogIn[ch];
    }
    return -1;
}


int execAnalogReset(int ch)
{
    if (ch >= 0 && ch < kMaxAnalogChannels) {
        analogTicker[ch].detach();
        analogBuffer[ch].count = 0;
        return 0;
    }
    return -1;
}


int execAnalogStart(int ch, long value)
{
    if (ch >=0 && ch < kMaxAnalogChannels) {
        analogBuffer[ch].count = 0;
        value *= 1000;
        if (value < kMinAnalogPeriod || value > kMaxAnalogPeriod) {
            value = 1000;    //  1 sec
        }
        switch (ch) {
            case 0: analogTicker[ch].attach_us(&analogTickerVector0, value);  break;
            case 1: analogTicker[ch].attach_us(&analogTickerVector1, value);  break;
            case 2: analogTicker[ch].attach_us(&analogTickerVector2, value);  break;
        }
        return 0;
    }

    return -1;
}


int execAnalogStop(int ch)
{
    if (ch >= 0 && ch < kMaxAnalogChannels) {
        analogTicker[ch].detach();
        return 0;
    }
    return -1;
}
















//
//  Digital Read
//
const int kMaxDigitalInChannels = 2;

DigitalIn digitalIn[kMaxDigitalInChannels] = {dp4,dp6};

int execDigitalIn(int ch)
{
    if (ch >= 0 && ch < kMaxDigitalInChannels) {
        return digitalIn[ch];
    }
    return -1;
}



//
//  Digital Write
//

const int kMaxDigitalOutChannels = 2;

DigitalOut digitalOut[kMaxDigitalOutChannels] = {dp26,dp28};

int execDigitalOut(int ch, long value)
{
    if (ch >= 0 && ch < kMaxDigitalOutChannels) {
        digitalOut[ch] = value == 0 ? 0 : 1;
        return 0;
    }
    return -1;
}


//
//  Pwm
//

const int kMaxPwmChannels = 3;

PwmOut Pwm[kMaxPwmChannels] = {dp1, dp2, dp18};

int execSetPwmPeriod(int ch, long value)
{
    if (ch >= 0 && ch < kMaxPwmChannels && value >= 0 && value <= kMaxInteger) {
        Pwm[ch].period_us(value);
        return 0;
    }
    return -1;
}

int execPwmOut(int ch, long value)
{
    if (ch >= 0 && ch < kMaxPwmChannels) {
        float f = value / fFloatScaleValue;
        Pwm[ch] = f;
        return 0;
    }

    return -1;
}




//
//  clock for RPi's timer and LED Flashing
//
Ticker second;
long onTimer  = -1;
long offTimer = -1;

void funcSecond()
{
    if (onTimer > 0) {
        onTimer --;
        if (onTimer == 0) {
            powerOn();
            onTimer = -1;
        }
    }
    if (offTimer > 0) {
        offTimer --;
        if (offTimer == 0) {
            powerOff();
            offTimer = -1;
        }
    }

    if (ledMode == kLED_Flash) {
        led = 1;
        wait_ms(10);
        led = 0;
    }
}



//
//  main routine
//

static char buf[MaxReceiveBufferSize];

int main()
{
    char strBuf[16];
    char prevCommand = 0;
    char prevCh      = 0;
    char prevMode    = 0;

    for (int i = 0; i < kMaxAnalogChannels; i++) {
        analogBuffer[i].count = 0;
    }
    
    slave.address(kCommandAddress);
    second.attach(funcSecond, 1);

    sw1.mode(PullNone);

    powerOn();

    while(1) {
        if (sw1 == SW_ON) {
            wait_ms(5);
            waitForSw1Release = 1;
        }

        if (waitForSw1Release && sw1 == SW_OFF) {
            if (powerMode == kCommandUp) {
                powerOff();
            } else {
                powerOn();
            }
            waitForSw1Release = 0;
        }

        int status = slave.receive();
        long value = 0;
        switch (status) {
            case I2CSlave::WriteAddressed: {
                if (slave.read(buf, MaxReceiveBufferSize)) {
                    char command = buf[0];
                    char ch      = buf[1];
                    char mode    = buf[2];
                    value = 0;
                    if (mode == kCommandModeBinary) {
                        long *p = (long *)buf+3;
                        value = *p;
                        if (value < 0) value = 0;
                    } else if (mode == kCommandModeASCII) {
                        value = atol(buf+3);
                    }

                    switch(command) {
                        case kCommandUp:
                            if (value > 0) {
                                onTimer = value;
                            } else if (value == 0) {
                                powerOn();
                            }
                            break;

                        case kCommandDown:
                            if (value > 0) {
                                offTimer = value;
                            } else if (value == 0) {
                                powerOff();
                            }
                            break;
                        case kCommandLED:
                            switch (mode) {
                                case 0:
                                    ledOff();
                                    break;
                                case 1:
                                    ledOn();
                                    break;
                                case 2:
                                    ledMode = kLED_Flash;
                                    break;
                            }
                            break;

                        case kCommandAnalogReset:
                            execAnalogReset(ch);
                            break;

                        case kCommandAnalogStart:
                            execAnalogStart(ch, value);
                            break;

                        case kCommandAnalogStop:
                            execAnalogStop(ch);
                            break;

                        case kCommandPwm:
                            execPwmOut(ch, value);
                            break;

                        case kCommandPwmPeriod:
                            execSetPwmPeriod(ch, value);
                            break;

                        case kCommandDigitalOut:
                            execDigitalOut(ch, value);
                            break;

                        case kCommandDigitalIn:
                        case kCommandAnalogRead:
                        case kCommandAnalogLoad:
                        case kCommandVersion:           //  ver 1.01
                            prevCommand = command;
                            prevCh      = ch;
                            prevMode    = mode;
                            break;
                        
                        default:
                            prevCommand = 0;
                            break;
                            
                    }
                }
                break;
            }

            case I2CSlave::ReadAddressed: {
                
                switch(prevCommand) {
                    case kCommandAnalogRead:
                        float f = execAnalogIn(prevCh);
                        if (prevMode == kCommandModeBinary) {
                            value = f * fFloatScaleValue;   // 10bit = 1024.0
                            slave.write((const char *)&value, sizeof(value));
                        } else {
                            sprintf(strBuf, "%f", f);
                            slave.write(strBuf, strlen(strBuf)+1);
                        }
                        break;

                    case kCommandAnalogLoad:     //  no ascii mode
                        if (prevCh < kMaxAnalogChannels) {
                            AnalogBuffer *p = &analogBuffer[prevCh];
//                            __disable_irq();
                            unsigned short count = p->count;
                            slave.write((const char *)p, sizeof(unsigned short)*(count+1));
//                            __enable_irq();
                            p->count = 0;
                        }
                        break;


                    case kCommandDigitalIn:
                        value = execDigitalIn(prevCh);
                        if (prevMode == kCommandModeBinary) {
                            slave.write((const char *)&value, sizeof(value));
                        } else {
                            sprintf(strBuf, "%ld", value);
                            slave.write((const char *)strBuf, strlen(strBuf)+1);
                        }
                        break;
                        
                    case kCommandVersion:                                       // ver 1.01
                        slave.write((const char *)&version, sizeof(version));
                        break;
                }
                slave.stop();
                break;
            }

            default:
                wait_ms(1);
                break;
        }
    }
}