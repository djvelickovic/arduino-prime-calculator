
#include <Wire.h>
#include <math.h>

// master states
#define MASTER_IDLE      0
#define MASTER_WORKING   1

// slave states
#define SLAVE_IDLE      0
#define SLAVE_RETURN    1 //?!
#define SLAVE_WORKING   2
#define SLAVE_UNKNOWN   3

// master constants
#define BUFFER_SIZE 50


// union for long numbers
typedef union {
  long value;
  byte avalue[4];
} LongNumber;

//// union for int numbers
//typedef union {
//  int value;
//  byte avalue[2];
//} int_number;

// slaves constants
const PROGMEM int slaveAddresses[] = { 8};
const PROGMEM String slaveNames[]   = { "SLAVE1"};
const PROGMEM byte slavesCnt = 1;

// slaves variables
//volatile byte slaveStates[]   = {SLAVE_UNKNOWN, SLAVE_UNKNOWN, SLAVE_UNKNOWN};
volatile byte slaveStates[]   = {SLAVE_IDLE};
volatile byte slavePercents[] = {0};
volatile long slaveTime[]     = {2};


// master variables

// primes
long primesBuffer[slavesCnt][BUFFER_SIZE];
long primes[slavesCnt] = {0};

// prime searching interval
LongNumber from;
LongNumber to;
LongNumber currentChunk;

// master current state
volatile byte state = MASTER_IDLE;


void setup() {
  Serial.begin(9600);
  Wire.begin();
}

void loop() {
  if (state == MASTER_WORKING) {
    Serial.println("WORKING ROUTINE");

    checkStatus();
    schedule();
    //checkPercent();
    checkResult();
    //printTime();
    printNumbers();
  }
  else if (state == MASTER_IDLE) {
    Serial.println("IDLE ROUTINE");
  }
  delay(2000);
}

// check each slave status and update variables
void checkStatus() {
  Serial.println("CHECKING STATUS");

  for (int i = 0; i < slavesCnt; i++) {
    Wire.beginTransmission(slaveAddresses[i]); // transmit to device
    Wire.write("STAT");                          // sends 4 bytes
    Wire.endTransmission();                      // ends transmission

    Wire.requestFrom(slaveAddresses[i], 1);    // request 1 bytes (state) from slave device

    if (Wire.available() < 1) {
      msgSlaveDead(i);
      //slaveStates[i] = SLAVE_UNKNOWN;
      continue;
    }
    if (Wire.available() > 1) {
      msgSlaveDrunk(i);
      dumpSerial();
      continue;
    }

    byte slaveState = Wire.read(); // reads state

    switch (slaveState) {
      case SLAVE_IDLE:
      case SLAVE_WORKING:
      case SLAVE_RETURN:
        Serial.print("Slave ");
        Serial.print(slaveNames[i]);
        Serial.print(" is in state ");
        Serial.println(slaveState);
        slaveStates[i] = slaveState;
        break;
      default:
        //slaveStates[i] = SLAVE_UNKNOWN;
        msgSlaveDrunk(i);
        break;
    }
  }
}

void checkPercent() {
  Serial.println("CHECKING PERCENT");

  for (int i = 0; i < slavesCnt; i++) {
    Wire.beginTransmission(slaveAddresses[i]); // transmit to device #8
    Wire.write("PERC");         // sends 4 bytes
    Wire.endTransmission();

    Wire.requestFrom(slaveAddresses[i], 1);    // request 1 bytes from slave device

    if (Serial.available() == 1) {
      byte percent = Wire.read();

      // checking validity
      if (percent < 0 || percent > 100) {
        msgSlaveDrunk(i);
        continue;
      }
      slavePercents[i] = percent;
      msgSlaveProgress(i);
    }
    else if (Serial.available() > 1 ) {
      Serial.print("Slave ");
      Serial.print(slaveNames[i]);
      Serial.println("is drunk!");
      // handle available (read until zero ?)
      continue;
    }
    else if (Serial.available() == 0 ) {
      Serial.print("Slave ");
      Serial.print(slaveNames[i]);
      Serial.println("is Dead!");
      continue;
    }
  }
}

void checkResult() {
  Serial.println("CHECKING RESULT");

  for (int i = 0; i < slavesCnt; i++){
    if (slaveStates[i] == SLAVE_RETURN){
        LongNumber r;
        Wire.requestFrom(slaveAddresses[i],4);
        r.avalue[0] = Wire.read();
        r.avalue[1] = Wire.read();
        r.avalue[2] = Wire.read();
        r.avalue[3] = Wire.read();
       while(r.value != 0){
          Wire.requestFrom(slaveAddresses[i],4);
          r.avalue[0] = Wire.read();
          r.avalue[1] = Wire.read();
          r.avalue[2] = Wire.read();
          r.avalue[3] = Wire.read();

          primesBuffer[i][primes[i]++] = r.value;

       }
    }
  }
}

void schedule() {
  Serial.println("SCHEDULING");
  if (currentChunk.value >= to.value){
      Serial.println("MASTER FINISHED SCHEDULING");
      return;
  }

  for (int i = 0; i < slavesCnt; i++) {

    if (slaveStates[i] == SLAVE_IDLE) { // only if slave is idle
      msgScheduling(i);

      LongNumber upperBound;
      upperBound.value = currentChunk.value + getDelta();
      upperBound.value = currentChunk.value > to.value ? to.value : currentChunk.value;

      Wire.beginTransmission(slaveAddresses[i]); // transmit to device
      Wire.write("TASK");         // sends 4 bytes
      Wire.write(from.value);     // sends 4 bytes
      Wire.write(to.value);       // sends 4 bytes
      Wire.endTransmission();

      currentChunk.value = upperBound.value;

      if (currentChunk.value >= to.value){
        break;
      }
    }
  }
}


// prints sorted estimated time for finishing slaves
void printTime() {
  int sorted[slavesCnt] = {};
  for (int i = 0; i < slavesCnt; i++) {
    sorted[i] = i;
  }

  //qsort(sorted, slaves,slaves,sort_desc);
  int tmp;
  for (int i = 0; i < slavesCnt - 1; i++) {
    for (int j = i + 1; j < slavesCnt; j++) {
      if (slaveTime[i] > slaveTime[j]) {
        tmp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = tmp;
      }
    }
  }

  for (int i = 0 ; i < slavesCnt; i++) {
    msgSlaveEta(sorted[i]);
  }
}

void printNumbers() {
  int sorted[slavesCnt] = {};
  for (int i = 0; i < slavesCnt; i++) {
    sorted[i] = i;
  }

  //qsort(sorted, slaves,slaves,sort_desc);
  int tmp;
  for (int i = 0; i < slavesCnt - 1; i++) {
    for (int j = i + 1; j < slavesCnt; j++) {
      if (primesBuffer[i][0] > primesBuffer[j][0]) {
        tmp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = tmp;
      }
    }
  }

  for (int i = 0 ; i < slavesCnt; i++) {
    printArray(i);
    primes[i] = 0;
  }
}


void serialEvent() {
  Serial.println(Serial.available());

  if (state == MASTER_IDLE) {
    char input[Serial.available()];
    byte pos = 0;
    while (Serial.available()) {
      input[pos++] = (char)Serial.read();
    }

    /* get the first token */
    char* token = strtok(input, " ");

    if (strcmp(token, "PROSTI")) {
      Serial.print("Unknown command '");
      Serial.println(token);
      return;
    }

    token = strtok(NULL, " ");
    from.value = atol(token);

    token = strtok(NULL, " ");
    to.value = atol(token);

    currentChunk.value = from.value;

    token = strtok(NULL, " ");
    if (token != NULL) {
      Serial.println("Invalid number of args!");
      return;
    }

    //DEBUG messages
    Serial.println(from.value);
    Serial.println(to.value);
    state = MASTER_WORKING;
  }
  // dump input
  dumpSerial();
}

long getDelta(){
  if (!(currentChunk.value >= to.value)){
    return -1;
  }
  long delta = 0;
  int a;
  for (long i = currentChunk.value; pi(i) - pi(currentChunk.value) < BUFFER_SIZE && i < to.value;){
    a = ((to.value - i) < 100) ? (to.value - i) : 100;
    i += a;
    delta += a;
  }
  return delta;
}


int pi(long n){
  return(n==0) ? 0 : (int) (n/log(n));
}




void dumpSerial(){
  while (Wire.available()) {
    Wire.read();
  }
}

void printArray(int i){
    for (int cnt = 0; cnt < primes[i];cnt++){
      Serial.println(primesBuffer[i][cnt]);
    }
}

void msgSlaveDrunk(int i){
      Serial.print("Slave ");
      Serial.print(slaveNames[i]);
      Serial.println("is drunk!");
}

void msgSlaveDead(int i){
    Serial.print("Slave ");
    Serial.print(slaveNames[i]);
    Serial.println("is dead!");
}

void msgSlaveEta(int i){
    Serial.print("Slave ");
    Serial.print(slaveNames[i]);
    Serial.print(" - ETA ");
    Serial.println(slaveTime[i]);
}

void msgScheduling(int i){
      Serial.print("Scheduling ");
      Serial.println(slaveNames[i]);
}


void msgSlaveProgress(int i){
      Serial.print("Slave ");
      Serial.print(slaveNames[i]);
      Serial.print(" progress is ");
      Serial.print(slavePercents[i]);
      Serial.println("%");
}
