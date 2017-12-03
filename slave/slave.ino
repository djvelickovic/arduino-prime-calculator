  
  #include <Wire.h>
  #include <math.h>
  
  #define WRK  1
  #define RTR  2
  #define IDL  3
  #define BUFFER_SIZE  100
  
  const int address = 8;
  
  int state = IDL;
  int percent = 0;
  long estimated_time = 0;
  
  long prime_buffer[BUFFER_SIZE] ;
  
  long buffer_length = 0;
  
  long start_number = 0;
  long end_number = 0;
  long current_number = 0;

  
  typedef union {
    long prime_number;
    byte primen_buffer[4];
  } prime_number;
  
  
  void setup() {
    // put your setup code here, to run once:
    Wire.begin(address);
    Wire.onReceive(receiveEvent);
    // Serial.begin(9600);
  
  }
  
  void loop() {
    if (state == WRK){
      if (current_number < end_number){
        if (isPrime(current_number)){
          prime_buffer[buffer_length] = current_number;
          buffer_length++;
        }
        current_number++;
      }
      else {
        state = RTR;
      }
    }
    else { // idle
      delay(100);
    }
  }

  bool isPrime(long number){
//    double number_sqrt = sqrt(number);
//    long lnsqrt = (long)number_sqrt;
    if (number % 2 == 0){
      return false;
    }
    for (long i = 3; i < number-1; i += 2){
      if (number % i == 0){
        return false;
      }
    }
    return true;
  }

  
  
  void receiveEvent(){
    String message = "";
    int avail = Wire.available();
    if (avail < 4){
      //ERROR
      return;
    }
    while (avail > 0){
      avail--;
      message += Wire.read();    
    }
    
    if (message.equals("TASK")){ // task
      // parse numbers
    }
    if (message.equals("STAT")){ // state
      if (state == WRK){
        Wire.write("WRK");
      }
      else if (state == RTR){
        Wire.write("RTR");
      }
      else if (state ==  IDL){
        Wire.write("IDL");
      }
    }
    else if (message.equals("RSLT")){ // result ?!??!@
      if (state == RTR){
        //RETURN NUMBER
      }
      else {
        Wire.write("F");
      }
    }
    else if (message.equals("PERC")){ // percent
      Wire.write(percent);
    }
    else if (message.equals("TIME")){ // time
      Wire.write(estimated_time);
    }
    
    
  }







