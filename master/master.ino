
#include <Wire.h>
#include <math.h>

// master states
#define MASTER_IDLE      0
#define MASTER_WORKING   1

// slave states

#define SLAVE_WORKING   0 
#define SLAVE_RETURN    1 
#define SLAVE_IDLE      2
#define SLAVE_UNKNOWN   3

// master constants
#define BUFFER_SIZE 50


// union for long numbers
typedef union {
  long value;
  byte avalue[4];
} LongNumber;


// slaves constants
const PROGMEM int slaveAddresses[] = { 8};
const PROGMEM String slaveNames[]  = { "SLAVE1"};
const PROGMEM byte slavesCnt       = 1;

int queue[slavesCnt * 2 + 1];
int qLen = 0;

// slaves variables
volatile byte slaveStates[]   = {SLAVE_IDLE};
//volatile byte slavePercents[] = {0};/
volatile long slaveTime[]     = {0};


// master variables

// primes
// buffer of primes per slave
long primesBuffer[slavesCnt][BUFFER_SIZE + 20];
// filled buffer per slave
long primes[slavesCnt] = {0};

// prime searching interval
//LongNumber from;
LongNumber to;
LongNumber currentChunk;

// master current state
volatile byte state = MASTER_IDLE;

// helper arrray for printing time
int sorted[slavesCnt] = {};

void setup() {
  Serial.begin(9600);
  Wire.begin();


  // setting up timer interrupt on every 1 second
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  OCR1A = 62500;
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS12);
  TIMSK1 |= (1 << OCIE1A);
  interrupts();

  // filling array with slave ids
  for (int i = 0; i < slavesCnt; i++) {
    sorted[i] = i;
  }
}

// timer interrupt routine
ISR(TIMER1_COMPA_vect) {

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



// master loop
void loop() {
  // gets status from slaves
  checkStatus();
  // gets eta from slaves
  checkTime();
  // schedule work to slaves
  schedule();
  // check result it some slave finished
  checkResult();
  // print numbers if appropriate buffer is full
  printNumbers();

  if (state == MASTER_IDLE) {
    delay(1500);
  }
  else {
    delay (100);
  }
}

// check each slave status and update status variables
void checkStatus() {
  //  Serial.println("CHECKING STATUS");/

  for (int i = 0; i < slavesCnt; i++) {
    Wire.beginTransmission(slaveAddresses[i]); // transmit to device
    Wire.write("STAT");                          // sends 4 bytes
    Wire.endTransmission();                      // ends transmission
    
    Wire.requestFrom(slaveAddresses[i], 1);    // request 1 bytes (state) from slave device
    if (Wire.available() < 1) {
      msgSlaveDead(i);
      slaveStates[i] = SLAVE_UNKNOWN;
      continue;
    }
    if (Wire.available() > 1) {
      msgSlaveDrunk(i);
      dumpWire();
      slaveStates[i] = SLAVE_UNKNOWN;
      continue;
    }
    // reads state
    slaveStates[i] = Wire.read();
  }
}


/**
 * Checks result if some slave is in return state
 */
void checkResult() {

  for (int i = 0; i < slavesCnt; i++) {
    if (slaveStates[i] == SLAVE_RETURN) {
      Wire.beginTransmission(slaveAddresses[i]);
      Wire.write("RSLT");
      Wire.endTransmission();
      LongNumber r;
      Wire.requestFrom(slaveAddresses[i], 4);
      r.avalue[0] = Wire.read();
      r.avalue[1] = Wire.read();
      r.avalue[2] = Wire.read();
      r.avalue[3] = Wire.read();
      while (r.value != 0) { // if number is zero, than there are no more primes in slave

        primesBuffer[i][primes[i]++] = r.value;

        Wire.beginTransmission(slaveAddresses[i]);
        Wire.write("RSLT");
        Wire.endTransmission();

        Wire.requestFrom(slaveAddresses[i], 4); 
        r.avalue[0] = Wire.read();
        r.avalue[1] = Wire.read();
        r.avalue[2] = Wire.read();
        r.avalue[3] = Wire.read();
      }
    }
  }
}

/**
 * Scheduler
 * Schedule job to first free slave witch is not in queue for printing
 */
void schedule() {
  if (currentChunk.value >= to.value) {
    return;
  }

  for (int i = 0; i < slavesCnt; i++) {

    if (slaveStates[i] == SLAVE_IDLE && !isInQueue(i)) { // only if slave is idle
      msgScheduling(i);

      LongNumber upperBound;
      upperBound.value = getNewUpperBound();

      Wire.beginTransmission(slaveAddresses[i]); // transmit to device
      Wire.write("TASK");         
      Wire.write(currentChunk.avalue[0]); // sends lower bound
      Wire.write(currentChunk.avalue[1]);
      Wire.write(currentChunk.avalue[2]);
      Wire.write(currentChunk.avalue[3]);

      Wire.write(upperBound.avalue[0]); // sends upper bound
      Wire.write(upperBound.avalue[1]);
      Wire.write(upperBound.avalue[2]);
      Wire.write(upperBound.avalue[3]);
      Wire.endTransmission();

      currentChunk.value = upperBound.value; // updates current chunk
      qAdd(i); // adds task in queue for printing
      
      if (currentChunk.value >= to.value) { // if finished, stop scheduling
        Serial.println("MASTER FINISHED SCHEDULING");
        state = MASTER_IDLE;
        break;
      }
    }
  }
}

/**
 * Checks ETA from slaves if in working state and updates values 
 */
void checkTime() {
  for (int i = 0; i < slavesCnt; i++) {

    if (slaveStates[i] == SLAVE_WORKING){
      Wire.beginTransmission(slaveAddresses[i]); // transmit to device
      Wire.write("TIME");         // sends 4 bytes
      Wire.endTransmission();
  
      Wire.requestFrom(slaveAddresses[i], 4);
      LongNumber eta;
      eta.avalue[0] = Wire.read();
      eta.avalue[1] = Wire.read();
      eta.avalue[2] = Wire.read();
      eta.avalue[3] = Wire.read();
  
      slaveTime[i] = eta.value;
    }
    else {
      slaveTime[i] = 0;
    }
  }
}


/**
 * Prints numbers in order from queue filled in scheduler
 */
void printNumbers() {
  if (qLen <= 0) {
    return;
  }
  while (1) {
    int head = qPeek();
    if (head == -1) {
      return;
    }
    if (primes[head] > 0) {
      printArray(head);
      primes[head] = 0;
      qPoll();
    }
    else {
      return;
    }
  }
}


/**
 * Reads input
 */
void serialEvent() {
//  Serial.println(Serial.available());/

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
    currentChunk.value = atol(token);

    token = strtok(NULL, " ");
    to.value = atol(token);

    token = strtok(NULL, " ");
    if (token != NULL) {
      Serial.println("Invalid number of args!");
      return;
    }

    state = MASTER_WORKING;
  }
  // dump input
  while (Serial.available()){
    Serial.read();
  }
}

/**
 * Calculates new chunk upper bound, if overflow, returns upperBound
 */
long getNewUpperBound() {
  for (long i = currentChunk.value; i < to.value; i += 50) {
    if (pi(i) - pi(currentChunk.value) >= BUFFER_SIZE) {
      return i;
    }
  }
  return to.value;
}

/**
 * Some poor function for aproximating number of primes
 */
long pi(long n) {
  return (n == 0) ? 0 : (long) (n / log(n));
}


void dumpWire() {
  while (Wire.available()) {
    Wire.read();
  }
}

void printArray(int i) {
  for (int cnt = 0; cnt < primes[i]; cnt++) {
    Serial.println(primesBuffer[i][cnt]);
  }
}

void msgSlaveDrunk(int i) {
  Serial.print("Slave ");
  Serial.print(slaveNames[i]);
  Serial.println(" is drunk!");
}

void msgSlaveDead(int i) {
  Serial.print("Slave ");
  Serial.print(slaveNames[i]);
  Serial.println(" is dead!");
}

void msgSlaveEta(int i) {
  if (slaveTime[i] > 0) {
    Serial.print("Slave ");
    Serial.print(slaveNames[i]);
    Serial.print(" - ETA: ");
    Serial.println(slaveTime[i]);
  }
}

void msgScheduling(int i) {
  Serial.print("Scheduling ");
  Serial.println(slaveNames[i]);
}

// QUEUE PART


int qPeek() {
  return qLen > 0 ? queue[0] : -1;
}

int qPoll() {
  if (qLen <= 0) {
    return -1;
  }
  int h = queue[0];
  for (int i = 1; i < qLen; i++) {
    queue[i - 1] = queue[i];
  }
  qLen--;
  return h;
}

int qAdd(int n) {
  queue[qLen++] = n;
}

bool isInQueue(int n) {
  for (int i = 0; i < qLen; i++) {
    if (queue[i] == n) {
      return true;
    }
  }
  return false;
}














