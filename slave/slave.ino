
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

const int address = 8;

volatile byte state = SLAVE_IDLE;
volatile byte percent = 0;
volatile long eta = 0;

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


void setup() {
  // put your setup code here, to run once:
  Wire.begin(address);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  Serial.begin(9600);

  //lowerBound = 1000000000;
  //upperBound = 1000000200;
  state = SLAVE_IDLE;
  //currentNumber = lowerBound;

  startedTime = millis();
  Serial.println("ZIV SAM");
}

void loop() {
  if (state == SLAVE_WORKING) {
    if (currentNumber < upperBound) {
      Serial.print("Checking ");
      Serial.println(currentNumber);

      if (isPrime(currentNumber)) {
        primeBuffer[bufferSize++] = currentNumber;
        Serial.println(currentNumber);
      }
      currentNumber++;
      currentTime = millis();

      double cspeed = (double)(currentTime-startedTime)/(double)(currentNumber - lowerBound);

      eta = cspeed*(upperBound - currentNumber);
      Serial.print("ETA: ");
      Serial.println(eta);

      double dpercent = 100.0/(upperBound - lowerBound) * (currentNumber - lowerBound);
      percent = (byte)dpercent;
      Serial.print("Percent ");
      Serial.println(percent);
    }
    else {
      Serial.println("Slave in return");
      state = SLAVE_RETURN;
      printAllNumbers();
    }
  }
  else { // idle
    delay(100);
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
  Serial.println(lastMessage);
  switch (lastMessage) {
    case TASK:
      if (state == SLAVE_WORKING) {
        Wire.write(OK);
      }
      else if (state == SLAVE_IDLE) {
        Wire.write(FAIL);
      }
      break;
    case STAT:
      Wire.write(state);
      break;
    case PERC:
      Wire.write(percent);
      break;
    case TIME:
      Wire.write(eta);
      break;
    case RSLT:
      if (state == SLAVE_RETURN) {
        if (bufferPosition < bufferSize) {
          Wire.write(primeBuffer[bufferPosition++]);
        }
        else {
          long resp = 0;
          Wire.write(resp);
          Serial.println("Slave in idle");
          state = SLAVE_IDLE;
          bufferPosition = 0;
          bufferSize = 0;
          eta = 0;
          percent = 0;
        }
      }
      break;
     default:
     Serial.println("Oh master what are you doing?!");
  }
}

void receiveEvent() {
  String message = "";
  int avail = Wire.available();
  if (avail < 4) {
    //ERROR
    return;
  }
  while (avail > 0) {
    avail--;
    message += (char)Wire.read();
  }
  Serial.println(message);

  if (message.equals("TASK")) { // task
    lastMessage = TASK;

    if (Wire.available() != 8) {
      while (Wire.available()) {
        Wire.read();
      }
    }

    LongNumber tmp;
    tmp.avalue[0] = Wire.read();
    tmp.avalue[1] = Wire.read();
    tmp.avalue[2] = Wire.read();
    tmp.avalue[3] = Wire.read();
    lowerBound = tmp.value;

    tmp.avalue[0] = Wire.read();
    tmp.avalue[1] = Wire.read();
    tmp.avalue[2] = Wire.read();
    tmp.avalue[3] = Wire.read();
    upperBound = tmp.value;

    Serial.println("Slave in Working");
    state = SLAVE_WORKING;
    startedTime = millis();
    Serial.println("STARTED");

  }
  if (message.equals("STAT")) { // state
    lastMessage = STAT;
  }
  else if (message.equals("RSLT")) { // result ?!??!@
    lastMessage = RSLT;
  }
  else if (message.equals("PERC")) { // percent
    lastMessage = PERC;
  }
  else if (message.equals("TIME")) { // time
    lastMessage = TIME;
  }

}
