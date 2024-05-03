// Including the required Arduino libraries
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <TinyGPS++.h>       // include TinyGPS++ library
#include <TimeLib.h>         // include Arduino time library
#include <SoftwareSerial.h>  // include software serial library

// Uncomment according to your hardware type
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
//#define HARDWARE_TYPE MD_MAX72XX::GENERIC_HW

// Defining size, and output pins
#define MAX_DEVICES 8
#define CS_PIN 9

TinyGPSPlus gps;

#define S_RX 3  // define software serial RX pin (No TX used)

SoftwareSerial SoftSerial(S_RX, -1);  // configure SoftSerial library

// Create a new instance of the MD_Parola class with hardware SPI connection
MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

#define time_offset -14400  // define a clock offset in seconds Note: (1 hour) ==> UTC + 1 (but in seconds)

// variable definitions
char Time[] = "TIME: 00:00:00";
char Date[] = "DATE: 00-00-2000";
byte last_second, Second, Minute, Hour, Day, Month;
int Year;

// variable defaults
bool timeIsSet = false;

int h, m, s;

int mill = 0;

int timeout = 0;
int lastMillis = 0;
int lastCheck = 0;

unsigned long currentMillis = 0;

volatile bool pulse = false;


void setup() {
  //Strictly utilized for debugging purposes.
  Serial.begin(115200);

  // Set the intensity (brightness) of the display (0-15)
  myDisplay.begin();
  myDisplay.setIntensity(0);
  myDisplay.displayClear();
  myDisplay.setTextAlignment(PA_LEFT);

  cli();  // Disable interrupts
    // Set Timer2 to interrupt every 1 ms
  TCCR2A = 0;  // Set entire TCCR2A register to 0
  TCCR2B = 0;  // Same for TCCR2B
  TCNT2 = 0;   // Initialize counter value to 0
  // Set compare match register to desired timer count.
  OCR2A = 249;  // 16MHz / 64 (prescaler) / 250 = 1000 Hz
  // Enable CTC mode
  TCCR2A |= (1 << WGM21);
  // Set prescaler to 64 and start the timer
  TCCR2B |= (1 << CS22);
  // Enable timer compare interrupt
  TIMSK2 |= (1 << OCIE2A);

  // Enable global interrupts
  sei();  // Enable interrupts

  attachInterrupt(digitalPinToInterrupt(2), PPS, RISING);  // Attach interrupt to pin 2, triggering on rising edge
}

// Triggered every 1ms, using timer2 interrupts for accurate (enough) timing of the milliseconds.
ISR(TIMER2_COMPA_vect) {
  if (timeIsSet) {
    mill++;  //increment by 1;
  }
}

void loop() {
  if (timeIsSet == false) {  //Determine whether the GPS is needed to set the initial time.
    timeIsSet = true;

    Serial.print("Time is being set...");

    SoftSerial.begin(9600);  // initialize software serial at 9600 baud

    getTime();

    SoftSerial.end();  // End it so that it does not interfere later...

    Serial.println("done!");

    myDisplay.setTextAlignment(PA_LEFT);

    h = String(hour()).toInt();
    m = String(minute()).toInt();
    s = String(second()).toInt();

    mill = 0;
    timeout = 0;
  }

  if (pulse) {
    pulse = false;
    timeout = 0;
    mill = 0;

    s++;
  }


  /*if (timeout >= 5000) {
    timeout = 0;
    timeIsSet = false;
    Serial.println("Timed out, acquiring new time from GPS...");
  }*/


  ////////////////// WORK WITH TIME ///////////////////

  if (s > 59) {
    s = 0;
    m++;
    mill = 0;
  }
  if (m > 59) {
    m = 0;
    h++;
    mill = 0;
  }
  if (h > 23) {
    h = 0;
    m = 0;
    s = 0;
    mill = 0;
  }

  printTime(String(h).toInt(), String(m).toInt(), String(s).toInt(), String(mill).toInt());
}

void printTime(int hr, int mn, int sc, int ms) {
  String hour_str = String(hr);
  String minute_str = String(mn);
  String second_str = String(sc);
  String millisecond_str = String(ms);

  // Format the data to fit on the display better, and increase readability;

  if (hr < 10) {
    hour_str = "0" + hour_str;
  }

  if (mn < 10) {
    minute_str = "0" + minute_str;
  }

  if (sc < 10) {
    second_str = "0" + second_str;
  }

  if (ms < 10) {
    millisecond_str = "0" + millisecond_str;
  } else if (ms < 100) {
    millisecond_str = "00" + millisecond_str;
  }

  String data = hour_str + ":" + minute_str + ":" + second_str + ":" + millisecond_str;

  myDisplay.print(data);  // Update the display with the relevant information.
}

void getTime() {
  bool completed = false;

  myDisplay.displayClear();
  myDisplay.setTextAlignment(PA_CENTER);
  myDisplay.print("Finding Sats.");

  while (!completed) {
    while (SoftSerial.available() > 0) {
      if (gps.encode(SoftSerial.read())) {

        // get time from GPS module
        if (gps.time.isValid()) {
          Minute = gps.time.minute();
          Second = gps.time.second();
          Hour = gps.time.hour();
        }

        // set currentMillis UTC time
        setTime(Hour, Minute, Second, Day, Month, Year);

        // add the offset to get local time
        adjustTime(time_offset);

        // update time array
        Time[12] = second() / 10 + '0';
        Time[13] = second() % 10 + '0';
        Time[9] = minute() / 10 + '0';
        Time[10] = minute() % 10 + '0';
        Time[6] = hour() / 10 + '0';
        Time[7] = hour() % 10 + '0';

        // update date array
        Date[14] = (year() / 10) % 10 + '0';
        Date[15] = year() % 10 + '0';
        Date[9] = month() / 10 + '0';
        Date[10] = month() % 10 + '0';
        Date[6] = day() / 10 + '0';
        Date[7] = day() % 10 + '0';

        const int sats = gps.satellites.value();

        Serial.print(F("Sats="));
        Serial.println(sats);

        if (sats > 1) {
          completed = true;
          myDisplay.displayClear();
        }
      }
    }
  }
}


void PPS() {
  pulse = true;  //Set a flag.
}
