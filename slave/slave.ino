
#include <Wire.h>
#include <math.h>

#define SLAVE_WORKING  0
#define SLAVE_RETURN   1
#define SLAVE_IDLE     2

#define TASK 0
#define STAT 1
#define PERC 2
#define RSLT 3
#define TIME 4

#define FAIL 0
#define OK 1

#define BUFFER_SIZE  100

typedef union {
  long value;
  byte avalue[4];
} LongNumber;


const PROGMEM int address = 8;

volatile byte state = SLAVE_IDLE;
volatile byte percent = 0;
volatile LongNumber eta;

long primeBuffer[BUFFER_SIZE] ;

volatile long bufferSize = 0;
volatile long bufferPosition = 0;

long lowerBound; // wrap in LongNumber before sending if error with memory!
long upperBound;
long currentNumber = 0;

long startedTime = 0;
long currentTime = 0;

long calcSpeed = 0;

volatile byte lastMessage;

double cspeed;

double dpercent;

void setup() {
  // put your setup code here, to run once:
  Wire.begin(address);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  Serial.begin(9600);

  state = SLAVE_IDLE;

  startedTime = millis();
  Serial.println("WAKING UP...");
}

void loop() {
  if (state == SLAVE_WORKING) {
    if (currentNumber < upperBound) {
      if (isPrime(currentNumber)) {
        primeBuffer[bufferSize++] = currentNumber;
      }
      currentNumber++;
      currentTime = millis();

      //calculating speed
      cspeed = (double)(currentTime-startedTime)/(double)(currentNumber - lowerBound);

      // calculating eta
      eta.value = cspeed*(upperBound - currentNumber);
      
//      Serial.print("ETA: ");
//      Serial.println(eta.value);

      dpercent = 100.0/(upperBound - lowerBound) * (currentNumber - lowerBound);
      percent = (byte)dpercent;
    }
    else {
      state = SLAVE_RETURN;
      //printAllNumbers();
    }
  }
  else { // idle
    delay(1000);
  }
}

void printAllNumbers(){
  for (int i = 0; i < bufferSize; i++){
    Serial.println(primeBuffer[i]);
  }
}

bool isPrime(long number) {
  if (number % 2 == 0) {
    return false;
  }

  double number_sqrt = sqrt(number);
  long lnsqrt = (long)number_sqrt;
  for (long i = 3; i < lnsqrt; i += 2) {
    if (number % i == 0) {
      return false;
    }
  }
  return true;
}


void requestEvent() {
  switch (lastMessage) {
    case TASK:
    // dont need handler
      break;
    case STAT:
      Wire.write(state);
      break;
    case PERC:
      Wire.write(percent);
      break;
    case TIME:
      Wire.write(eta.avalue[0]);
      Wire.write(eta.avalue[1]);
      Wire.write(eta.avalue[2]);
      Wire.write(eta.avalue[3]);

      break;
    case RSLT:
      if (state == SLAVE_RETURN) {
        if (bufferPosition < bufferSize) {
          LongNumber l;
          l.value = primeBuffer[bufferPosition++];
          Wire.write(l.avalue[0]);
          Wire.write(l.avalue[1]);
          Wire.write(l.avalue[2]);
          Wire.write(l.avalue[3]);

        }
        else {
          LongNumber resp;
          resp.value = 0;
          Wire.write(resp.avalue[0]);
          Wire.write(resp.avalue[1]);
          Wire.write(resp.avalue[2]);
          Wire.write(resp.avalue[3]);

          state = SLAVE_IDLE;
          bufferPosition = 0;
          bufferSize = 0;
          eta.value = 0;
          percent = 0;

          Serial.print(millis());
          Serial.print(": Finished work in ");
          Serial.println(millis()-startedTime);
        }
      }
      break;
     default:
     Serial.println("Oh master what are you doing?!");
  }
}

void receiveEvent() {
  String message = "";
  int avail = 4;
  if (Wire.available() < 4) {
    //ERROR
    return;
  }
  while (avail > 0) {
    avail--;
    message += (char)Wire.read();
  }

  if (message.equals("STAT")) {
    lastMessage = STAT;
  }
  else if (message.equals("RSLT")) {
    lastMessage = RSLT;
  }
  else if (message.equals("TIME")) {
    lastMessage = TIME;
  }
  else if (message.equals("TASK")) {

    if (state == SLAVE_WORKING){
      while (Wire.available()){
        Wire.read();
      }
    }
    
    lastMessage = TASK;

    LongNumber tmp;
    tmp.avalue[0] = Wire.read();
    tmp.avalue[1] = Wire.read();
    tmp.avalue[2] = Wire.read();
    tmp.avalue[3] = Wire.read();
    lowerBound = tmp.value;
    currentNumber = lowerBound;
    
    tmp.avalue[0] = Wire.read();
    tmp.avalue[1] = Wire.read();
    tmp.avalue[2] = Wire.read();
    tmp.avalue[3] = Wire.read();
    upperBound = tmp.value;

    //TODO: Check invalid input
    
    state = SLAVE_WORKING;
    startedTime = millis();
    Serial.print(startedTime);
    Serial.println(": Started work...");
  }
}
