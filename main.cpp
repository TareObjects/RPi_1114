#include "mbed.h"


#define kCommandAddress 0x50
#define MaxReceiveBufferSize 100

#define kCommandUp      'u'     //  i2c command : up - power on
#define kCommandDown    'd'     //              : down - power off
#define kCommandLED     'l'     //              : led - on/off/flash

#define kLED_Off    0   // LED off
#define kLED_On     1   // LED on
#define kLED_Flash  2   // LED flash 


I2CSlave slave(dp5, dp27);

DigitalOut rpiPower(dp17);
DigitalOut led(dp14);

DigitalIn sw1(dp25);
const int SW_OFF = 1;
const int SW_ON  = 0;   // pull up
int waitForSw1Release = 0;


int powerMode = kCommandUp;
int ledMode   = kLED_Flash;
int sw1Mode   = SW_OFF;


static char buf[MaxReceiveBufferSize];

Ticker second;
long onTimer  = -1;
long offTimer = -1;

void powerOff() {
    rpiPower  = 0;
    powerMode = kCommandDown;
}

void powerOn() {
    rpiPower  = 1;
    powerMode = kCommandUp;
}


void ledOn() {
    led     = 1;
    ledMode = kLED_On;
}

void ledOff() {
    led     = 0;
    ledMode = kLED_Off;
}


void funcSecond() {
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

int main()
{
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
        long t = 0;
        switch (status) {
            case I2CSlave::WriteAddressed:
                if (slave.read(buf, MaxReceiveBufferSize)) {
                    switch(buf[0]) {
                    case kCommandUp:
                        t = atol(buf+1);
                        if (t > 0) {
                            onTimer = t;
                        } else if (t == 0) {
                            powerOn();
                        }
                        break;
                        
                    case kCommandDown:
                        t = atol(buf+1);
                        if (t > 0) {
                            offTimer = t;
                        } else if (t == 0) {
                            powerOff();
                        }
                        break;    
                    case kCommandLED:
                        switch (buf[1]) {
                        case '0': ledOff();             break;
                        case '1': ledOn();              break;
                        case 'f': ledMode = kLED_Flash; break;
                        }
                    }
                }
                break;

            default:
                wait_ms(1);
                break;
        }
    }
}

