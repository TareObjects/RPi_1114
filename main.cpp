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

int powerMode = kCommandUp;
int ledMode   = kLED_Flash;


static char buf[MaxReceiveBufferSize];

Ticker second;
long counter = -1;

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
    if (counter > 0) {
        counter --;
        if (counter == 0) {
            if (powerMode == kCommandDown) {
                powerOn();
            } else {
                powerOff();
            }
            counter = -1;      
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
    
    powerOn();
    
    while(1) {
        int status = slave.receive();
        switch (status) {
            case I2CSlave::WriteAddressed:
                if (slave.read(buf, MaxReceiveBufferSize)) {
                    switch(buf[0]) {
                    case kCommandUp:
                        if (powerMode == kCommandUp) {
                            // error already up   
                        } else {
                            long t = atol(buf+1);
                            if (t > 0) {
                                counter = t;
                            } else if (t == 0) {
                                powerOn();
                            }
                        }
                        break;
                        
                    case kCommandDown:
                        if (powerMode == kCommandDown) {
                            // error already down   
                        } else {
                            long t = atol(buf+1);
                            if (t > 0) {
                                counter = t;
                            } else if (t == 0) {
                                powerOn();
                            }
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

