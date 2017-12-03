
  #include <Wire.h>

  // union for long numbers
  typedef union {
    long value;
    byte avalue[4];
  } long_number;

// union for int numbers
  typedef union {
    int value;
    byte avalue[2];
  } int_number;

  // master states
  #define MASTER_IDLE      0
  #define MASTER_WORKING   1

  // slave states
  #define SLAVE_IDLE      0
  #define SLAVE_RETURN    1 ?!
  #define SLAVE_WORKING   2
  #define SLAVE_UNKNOWN   3

  // master constants
  #define CHUNK_SIZE 200
  #define BUFFER_SIZE 300

  // slaves constants
  const PROGMEM int slaves_addresses[] = { 8, 9, 10};
  const PROGMEM String slaves_names[]   = { "SLAVE1", "SLAVE2", "SLAVE3" };
  const PROGMEM byte slaves = 3;
  
  // slaves variables
  volatile byte slaves_states[]   = {SLAVE_UNKNOWN,SLAVE_UNKNOWN,SLAVE_UNKNOWN};
  volatile byte slaves_percents[] = {0,   0,   0  };
  volatile long slaves_time[]     = {2,   5,   0  };


  // master variables
  
  // primes
  long prime_buffer[BUFFER_SIZE] = {};
  long primes = 0;

  // prime searching interval
  long_number from;
  long_number to;
  long_number current_chunk;

  // master current state
  volatile byte state = MASTER_IDLE;
  
  

  
  void setup() {
    Serial.begin(9600);
    Wire.begin();
  }
  
  void loop() {
    if (state == MASTER_WORKING){
      Serial.println("WORKING ROUTINE");

      check_stat();
      schedule();
      check_percent();
      check_result();
      printPercents();
    }
    else if (state == MASTER_IDLE){
      Serial.println("IDLE ROUTINE");
    }
    delay(2000);
  }

  // check each slave status and update variables
  void check_stat(){
    Serial.println("CHECKING STATUS");
    
    for (int i = 0; i < slaves; i++){
      Wire.beginTransmission(slaves_addresses[i]); // transmit to device
      Wire.write("STAT");                          // sends 4 bytes
      Wire.endTransmission();                      // ends transmission

      
      Wire.requestFrom(slaves_addresses[i], 1);    // request 1 bytes (state) from slave device

      if (Serial.available() < 1){
        Serial.printf("Slave %s is dead!\n",slaves_names[i]);
        continue;
      }
      if (Serial.available() > 1){
        Serial.print("Slave %s is drunk!\n",slaves_names[i]);
        continue;
      }
      
      byte slave_state = Wire.read(); // reads state
      
      switch(slave_state){
        case SLAVE_IDLE:
          Serial.printf("Slave %s is IDLE",slaves_names[i]);
          break;
         case SLAVE_WORKING:
          Serial.printf("Slave %s is IDLE",slaves_names[i]);
          break;
         case SLAVE_RETURNING:
          Serial.printf("Slave %s is RETURNING",slaves_names[i]);
          break;
         default:
          Serial.printf("Slave %s is fucked up",slave_names[i]);
          continue;  
      }
      
      slaves_states[i] = slave_state; // sets state
    }
  }

  void check_percent(){
    Serial.println("CHECKING PERCENT");
    
    for (int i = 0; i < slaves; i++){
      Wire.beginTransmission(slaves_addresses[i]); // transmit to device #8
      Wire.write("PERC");         // sends 4 bytes
      Wire.endTransmission();

      Wire.requestFrom(slaves_addresses[i], 1);    // request 1 bytes from slave device

      if (Serial.available() == 1){
        byte percent = Wire.read();
      
        // checking validity
        if (percent < 0 || percent > 100){
          Serial.print("Slave ");
          Serial.print(slaves_names[i]);
          Serial.print(" is drunk. Percent is fucked up.\n");
        }
        else {
          Serial.print("Slave ");
          Serial.print(slaves_names[i]);
          Serial.print(" progress is ");
          Serial.print(percent);
          Serial.print("%\n");

          slaves_percents[i] = percent;
        }
      }
      else if (Serial.available() > 1 ){ 
        Serial.printf("Slave %s is drunk!", slaves_names[i]);
        // handle available (read until zero ?)
        continue;
      }
      else if (Serial.available() == 0 ){
        Serial.printf("Slave %s is dead!",slaves_names[i]);
        continue;
      }
    }
  }

  void check_result(){
    Serial.println("CHECKING RESULT");
  }

  void schedule(){
    Serial.println("SCHEDULING");
    
    for (int i = 0; i < slaves; i++){
       if (slaves_states[i] == SLAVE_IDLE){ // only if slave is idle
          Serial.print("Scheduling %s ",slaves_names[i]);
        
          Wire.beginTransmission(slaves_addresses[i]); // transmit to device #8
          Wire.write("TASK");         // sends 4 bytes
          
          Wire.write(x);     /         // sends 4 bytes
          Wire.write(y);        /      // sends 4 bytes
          Wire.endTransmission(); 
       }
    }
  }


  // qsort requires you to create a sort function
  int sort_desc(const int *cmp1, const int *cmp2)
  {
    // Need to cast the void * to int *
    int a = *cmp1;
    int b = *cmp2;
    // The comparison
    Serial.print("SortDesc ");
    Serial.print(a);
    Serial.print(" ");
    Serial.println(b);
    return slaves_time[a] > slaves_time[b] ? -1 : (slaves_time[a] < slaves_time[b] ? 1 : 0);
    // A simpler, probably faster way:
    //return b - a;
  }
  
  void printPercents(){
    int sorted[slaves] = {};
    for (int i = 0; i < slaves; i++){
      sorted[i] = i;
    }


    //qsort(sorted, slaves,slaves,sort_desc);
    int tmp;
    for (int i = 0; i < slaves -1; i++){
      for (int j = i+1; j < slaves;j++){
        if (slaves_time[i] > slaves_time[j]){
          tmp = sorted[i];
          sorted[i] = sorted[j];
          sorted[j] = tmp;
        }
      }
    }
    
    for (int i = 0 ; i < slaves; i++){
      Serial.print("Slave ");
      Serial.print(slaves_names[sorted[i]]);
      Serial.print(", remaining time: ");
      Serial.print(slaves_time[sorted[i]]);
      Serial.print("\n");
    }
  }

  void printNumbers(){
    
  }
  
  
  void serialEvent(){
    Serial.println(Serial.available());
    
    if (state == MASTER_IDLE){ 
      char input[Serial.available()];
      byte pos = 0;
      while (Serial.available()){
        input[pos++] = (char)Serial.read();
      }

        /* get the first token */
      char* token = strtok(input," ");

      if (strcmp(token,"PROSTI")){
        Serial.print("Unknown command '");
        Serial.print(token);
        Serial.print("'\n");
        return;
      }

//       /* walk through other tokens */
//       while( token != NULL ) {
//          Serial.println(token);
//          token = strtok(NULL, " ");
//       }

      token = strtok(NULL, " ");
      from = atol(token);

      token = strtok(NULL, " ");
      to = atol(token);

      token = strtok(NULL, " ");
      if (token != NULL){
        Serial.println("Invalid number of args!");
        return;
      }
      
      Serial.println(from);
      Serial.println(to);
      state = MASTER_WORKING;
    }
    else { // dump input
      while (Serial.available()){ Serial.read();}
    }
  }



























  
