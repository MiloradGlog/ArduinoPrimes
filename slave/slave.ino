#include <Wire.h>
#include <QueueList.h>

#define ADDRESS 8


#define STATE_FREE 1
#define STATE_WORKING 2
#define STATE_DONE 3
#define STATE_SENDING_P1 4
#define STATE_SENDING_P2 5
#define STATE_SENDING_P3 6
#define MASTER_REQUEST_SEND_NUMBER_OF_PRIMES 10
#define MASTER_REQUEST_SEND_ALL_PRIMES 11
#define MASTER_REQUEST_SET_MASTER 12
#define MASTER_REQUEST_RESET_OWNERSHIP 13
#define MASTER_REQUEST_RECIEVE_PACKAGE 14

#define NO_MASTER 0
/*
 * x i y su pocetni i krajnji broj opsega
 * packageNum je redni broj poslatog paketa
 */
typedef struct Package{
  long x;
  long y;
  int packageNum;
}Package;

Package package;

byte STATE = STATE_FREE;

byte CURRENT_MASTER = NO_MASTER;

int bufferSize;
int sendingCounter = 0;

QueueList<long> bufferQueue;


void setup() {
  
  Serial.begin(9600);
  Wire.begin(ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  bufferQueue.setPrinter(Serial);
  
}
void loop() {
  
  if (STATE == STATE_WORKING){
    mainTask();
  }

  
}



/*
 * Funkcija koja hendluje primljeni event, tj primljenu poruku od mastera
 */
void receiveEvent() {
   Serial.println("DOBIO SAM NEKI EVENT");
   int REQUEST = Wire.read();
   Serial.println(REQUEST);

   if (REQUEST == MASTER_REQUEST_RESET_OWNERSHIP){
     Serial.println("Oslobadjam roba od mastera");
     CURRENT_MASTER = 0;
     return;
   }
   
   if (REQUEST == MASTER_REQUEST_SET_MASTER){
     CURRENT_MASTER = Wire.read();
     Serial.print("Setovao sam trenutnog mastera da bude: ");
     Serial.println(CURRENT_MASTER);
     return;
   }
   
   switch(STATE){
     case STATE_FREE:{
       if (REQUEST == MASTER_REQUEST_RECIEVE_PACKAGE){
         readPackage();
         STATE = STATE_WORKING;
       }
       break;
     }
     case STATE_DONE:{
       if (REQUEST == MASTER_REQUEST_SEND_NUMBER_OF_PRIMES){
         STATE = STATE_SENDING_P1;
       }
       else{
         Serial.println("Greska u recieveEvent()->STATE_DONE");
       }
       break;
     }
     case STATE_SENDING_P2:{
       if (REQUEST == MASTER_REQUEST_SEND_ALL_PRIMES){
         STATE = STATE_SENDING_P3;
       }
       else{
         Serial.println("Greska u recieveEvent()->STATE_SENDING_2");
       }
       break;
     }
     default:{
      break;
     }
   }
}
/*
 * Funkcija koja hendluje request event
 */
void requestEvent(){

  Serial.print("----STIGAO REQUEST----STATE JE: ");
  Serial.println(STATE);
  
  switch(STATE){
    case STATE_WORKING:{
      Wire.write(STATE);
      break;
    }
    case STATE_FREE:{
      Wire.write(STATE);
      Wire.write(CURRENT_MASTER);
      break;
    }
    case STATE_DONE:{
      Wire.write(STATE);
      break;
    }
    case STATE_SENDING_P1:{
      sendSize();
      break;
    }
    case STATE_SENDING_P2:{
      Wire.write(STATE);
      
      break;
    }
    case STATE_SENDING_P3:{
      sendNumbers();
    }
    default:{
      break;
    }
  }
}





/*
 * Funkcija predstavlja glavni zadatak slave-a koji se izvrsava kad nema interrupta i slave ima sta da radi
 */
void mainTask(){
  if (package.x < package.y){
    Serial.print("Radim main task za X = ");
    Serial.print(package.x);
    Serial.print(", i Y = ");
    Serial.println(package.y);
    
    
    if (isPrime(package.x)){
      bufferQueue.push(package.x);
    }
    package.x++;
  }
  else{
    Serial.println("Menjam state u done");
    STATE = STATE_DONE;
  }
}

/*
 * Funkcija racuna da li je zadat long prost broj
 * Vraca true ako jeste i false ako nije
 */
boolean isPrime(long l){
  if (l == 1) return true;
  if (l == 2) return true;
  if (l % 2 == 0) return false;
  for (int i = 3; i < (l/2) + 1; i++){
    if (l % i == 0) return false;
  }
  return true;
}




/*
 * Funkcija masteru salje broj prostih za slanje
 */
void sendSize(){
  //Sending package size
  Wire.write(ADDRESS);
  Wire.write(package.packageNum);
  if(bufferQueue.isEmpty()){
    Wire.write(0);
    STATE = STATE_FREE;
  }
  else{
    Wire.write((byte)bufferQueue.count());
    STATE = STATE_SENDING_P2;
  }
}

/*
 * 
 */
void sendNumbers(){
  Serial.print("Radim sendNumbers() za velicinu buffera: ");
  Serial.println(bufferQueue.count());
  long l = bufferQueue.pop();
  
  Serial.print("Saljem: ");
  Serial.println(l);
  sendLong(l);
  
  if (bufferQueue.isEmpty()){
    Serial.println("Menjam state u FREE");
    STATE = STATE_FREE;
  }
  
  
}

/*
 * Funkcija koja cita paket od mastera, poziva se kada recieveEvent() prepozna da nam master salje nov paket
 * za rad, izvrsava se samo ako je trenutni posao zavrsen
 */
void readPackage(){

  if (STATE == STATE_WORKING){
    Serial.println("Vec radim, ne mogu da primim novi paket");
    return;
  }
  byte x[4];
  
  x[0] = Wire.read();
  x[1] = Wire.read();
  x[2] = Wire.read();
  x[3] = Wire.read();
  package.x = parseBytesToLong(x);
  
  x[0] = Wire.read();
  x[1] = Wire.read();
  x[2] = Wire.read();
  x[3] = Wire.read();
  package.y = parseBytesToLong(x);

  package.packageNum = Wire.read();
  printPackage(package);
  STATE = STATE_WORKING;
}


/*
 * Funkcija koja salje jedan long masteru
 * NISAM SIGURAN KOLIKO JE POTREBNA TRENUTNO
 */
void sendLong(long l){

  byte bytes[4];
  parseLongToBytes(l, bytes);

  Serial.println(bytes[0]);
  Serial.println(bytes[1]);
  Serial.println(bytes[2]);
  Serial.println(bytes[3]);  

  
  Wire.write(bytes[0]);
  Wire.write(bytes[1]);
  Wire.write(bytes[2]);
  Wire.write(bytes[3]);
  
}

/*
 * Funkcija koja parsira dat niz bajtova u long
 * NIJE TESTIRANO
 */

long parseBytesToLong(byte *b){

  long l = (((long)b[0] << 24) 
              + ((long)b[1] << 16) 
              + ((long)b[2] << 8) 
              + ((long)b[3] ) );

  return l;
  
}


/*
 * Funckija koja parsira dat long a i smesta ga u takodje prosledjeni niz bajtova x
 */

void parseLongToBytes(unsigned long a, byte *x){
  x[0] = (byte)((a >> 24) & 0xFF);
  x[1] = (byte)((a >> 16) & 0xFF);
  x[2] = (byte)((a >> 8) & 0XFF);
  x[3] = (byte)((a & 0XFF));

}


void printPackage(Package p){
  Serial.print("Paket broj: ");
  Serial.print(p.packageNum);
  Serial.println();
  Serial.print("X je: ");
  Serial.println(p.x);
  Serial.print("Y je: ");
  Serial.println(p.y);
}

