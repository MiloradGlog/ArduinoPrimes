#include <Wire.h>
#include <QueueList.h>

#define MIN_NUMBER_OF_PACKAGES 10
#define MIN_RANGE 5
#define NUMBER_OF_SLAVES 1 
#define MAX_NUMBER_OF_SLAVES 1
#define MAX_BUFFER_SPACE 1024
#define DEBUG 0

#define SLAVE_STATE_FREE 1
#define SLAVE_STATE_WORKING 2
#define SLAVE_STATE_DONE 3
#define SLAVE_STATE_SENDING_P1 4
#define SLAVE_STATE_SENDING_P2 5
#define SLAVE_STATE_SENDING_P3 6

#define REQUEST_SEND_NUMBER_OF_PRIMES 10
#define REQUEST_SEND_ALL_PRIMES 11
#define REQUEST_SET_MASTER 12
#define REQUEST_RESET_OWNERSHIP 13
#define REQUEST_RECIEVE_PACKAGE 14
 
#define SLAVE_NO_MASTER 0
#define MASTER_ID 1


/*
 * SREDI PAKETE QUEUELIST I SREDI BUFFER U MASTERU I PREDAJ JEBENI PROJEKAT
 */
typedef struct Package{
  byte x[4];
  byte y[4];
  int packageNum;
}Package;

typedef struct BufferedInput{
  QueueList<long> numbers;
  int slaveAddress;
  int packageNum;
}BufferedInput;


BufferedInput bufferedInputs[NUMBER_OF_SLAVES];


int MAX_RANGE = MAX_BUFFER_SPACE/sizeof(long)/NUMBER_OF_SLAVES;
int NUMBER_OF_PACKAGES = 0;
int LAST_PRINTED_PACKAGE = 0;
int MAX_PACKAGE_BUFFER = 512 / sizeof(Package);

boolean IS_ACTIVE = false;

long x = -1;
long y = -1;

QueueList<Package> packages;

int AVAILABLE_SLAVE_ADDRESSES[NUMBER_OF_SLAVES]; 
int ALL_SLAVE_ADDRESSES[MAX_NUMBER_OF_SLAVES] = {8};

int NUMBER_OF_AVAILABLE_SLAVES = 0;

int CURRENT_INPUT_PACKAGE_NUMBER = 0;
int CURRENT_INPUT_ADDRESS = 0;
int CURRENT_INPUT_SIZE = 0;

int RELEASE_TIMEOUT = -1;

void setup() {
  Wire.begin();
  Serial.begin(9600);
  printWelcomeMessage();
}


/* MOGUCE FUNKCIJE:
 *  
 * parseInputFromSerial()
 * charToInt()
 * skipSpaceSerial()
 * getLongFromSerial()
 * printStatus()
 * 
 * generateTokens()
 * printTokens()
 * parseBytesToX(*Package p)
 * parseBytesToY(*Package p)
 * parseLongToBytes(long a, long b, *Package p)
 * parseBytesToLong(byte b1, byte b2, byte b3, byte b4)
 * 
 * 
 * sendPackage(*p, slaveAddress)
 */

void serialEvent(){
  if (IS_ACTIVE){
    Serial.println("Trenutno radim, molim Vas da sacekate.");
    flushSerial();
    return;
  }
  delay(500);
  determineAvailableSlaves();
  startReadingSerial();
  delay(50);
}

void checkActive(){
  if (NUMBER_OF_PACKAGES > 0){
    IS_ACTIVE = true;
  }
  else{
    if(IS_ACTIVE){
      RELEASE_TIMEOUT = 50;
    }
    IS_ACTIVE = false;
  }
}

void loop() {

  checkAllSlaves();
  checkActive();
  checkReleaseTimeout(); 
  delay(50);
}


void checkReleaseTimeout(){
  if (RELEASE_TIMEOUT < -1){
    RELEASE_TIMEOUT = -1;
  }
  else if (RELEASE_TIMEOUT == 0){
    endEvent();
    RELEASE_TIMEOUT--;
  }
  else if (RELEASE_TIMEOUT > 0){
    RELEASE_TIMEOUT--;
  }
//  Serial.print("Release timer je: ");
//  Serial.println(RELEASE_TIMEOUT);
}


void checkAllSlaves(){
  for (int i = 0; i < NUMBER_OF_AVAILABLE_SLAVES; i++){
    //PROMENI U AVAILABLE KAD ZAVRSIS
    checkSlave(AVAILABLE_SLAVE_ADDRESSES[i]);
  }
}

/*
 * Proverava slave-a u kom je stanju, i hendluje njegovo stanje
 */
void checkSlave(int slaveAddress){

  Wire.requestFrom(slaveAddress, 1);
  int sw = Wire.read();
  switch(sw){
    case SLAVE_STATE_FREE:{
//      Serial.println("SLAVE JE FREE");
     // Wire.read();
      if (!packages.isEmpty()) sendNextPackage(slaveAddress);
      break;
      
    }
    case SLAVE_STATE_WORKING:{
      //Serial.println("SLAVE JE WORKING");
      break;
    }
    case SLAVE_STATE_DONE:{
//      Serial.println("SLAVE JE DONE");
      requestBufferSize(slaveAddress);
      break;
    }
    case SLAVE_STATE_SENDING_P1:{
//      Serial.println("SLAVE JE SND1");
      //mozda ovde treba request numbers
      break;
    }
    case SLAVE_STATE_SENDING_P2:{
//      Serial.println("SLAVE JE SND2");
      requestNumbers(slaveAddress);
      break;
    }
    default:{
      break;
    }
  }
  
}


/*
 * Funkcija koja od slave-a requestuje size buffera koji ce se poslati
 */
void requestBufferSize(int slaveAddress){

  Wire.beginTransmission(slaveAddress);
  Wire.write(REQUEST_SEND_NUMBER_OF_PRIMES);
  Wire.endTransmission();

  Wire.requestFrom(slaveAddress, 3);

  CURRENT_INPUT_ADDRESS = Wire.read();
  CURRENT_INPUT_PACKAGE_NUMBER = Wire.read();
  CURRENT_INPUT_SIZE = Wire.read();
  
  NUMBER_OF_PACKAGES--;
  
  if (DEBUG){
    Serial.println("\n----Stize paket----: ");
    Serial.print("ADRESA: ");
    Serial.println(CURRENT_INPUT_ADDRESS);
    Serial.print("REDNI BROJ PAKETA: ");
    Serial.println(CURRENT_INPUT_PACKAGE_NUMBER);
    Serial.print("VELICINA PAKETA: ");
    Serial.println(CURRENT_INPUT_SIZE);
    Serial.print("NUMBER OF PACKAGES LEFT: ");
    Serial.println(NUMBER_OF_PACKAGES);
  }


}

/*
 * Funckija koja od slave-a trazi sve izracunate brojeve i smesta ih u buffer ako im nije vreme za ispis
 * Trazi onoliko brojeva koliko je requestBufferSize() dobila kao odgovor
 */
void requestNumbers(int slaveAddress){

  BufferedInput in;

  in.packageNum = CURRENT_INPUT_PACKAGE_NUMBER;
  in.slaveAddress = CURRENT_INPUT_ADDRESS;

  

//  promeni if da nije return nego da preskace sledeci blok
  if (CURRENT_INPUT_SIZE < 1) return;
  
  Wire.beginTransmission(slaveAddress);
  Wire.write(REQUEST_SEND_ALL_PRIMES);
  Wire.endTransmission();


  if(DEBUG){
    Serial.print("Broj brojeva koje ocekujem u requestNumbers() = ");
    Serial.println(CURRENT_INPUT_SIZE);
  }
  
  for (int i = 0; i < CURRENT_INPUT_SIZE; i++){
    
    Wire.requestFrom(slaveAddress, 4);
    long number = recieveLong();
    in.numbers.push(number);
//    Serial.print("Pushovao sam broj: ");
//    Serial.print(number);
//    Serial.print(", sada ima brojeva: ");
//    Serial.println(in.numbers.count());
  }
  
  //dodaj sortiranje
  printInputPackage(&in);
}


void printInputPackage(BufferedInput *in){
    LAST_PRINTED_PACKAGE = in->packageNum;
    while(!in->numbers.isEmpty()){
      Serial.println(in->numbers.pop());
    }
}
/*
 * Funkcija koja zapocinje proces citanja iz serijala i generise pakete za slanje
 */
void startReadingSerial(){

  parseInputFromSerial();
  checkOverflow();
  printStatus();
  generatePackages();
  if (DEBUG) {
    printPackages();
  }
  
}

/*
 * DOBRO POGLEDAJ OVU FUNKCIJU I NASTAVI OD NJE RAD
 * NESTO NE STIMA SA BROJEM ADRESA I SLAVEOVA
 */
void determineAvailableSlaves(){
  for (int i = 0; i < MAX_NUMBER_OF_SLAVES; i++){
    
    Wire.requestFrom(ALL_SLAVE_ADDRESSES[i], 2);
    int SLAVE_STATE = Wire.read();
    Serial.println(SLAVE_STATE);
    if (SLAVE_STATE == SLAVE_STATE_FREE){
      byte SLAVE_MASTER = Wire.read();
    Serial.println(SLAVE_MASTER);
      if (SLAVE_MASTER == 0 || SLAVE_MASTER == MASTER_ID){
        setSlaveMaster(ALL_SLAVE_ADDRESSES[i]);
      }
    }
  }
}


void setSlaveMaster(int slaveAddress){
  Wire.beginTransmission(slaveAddress);
  Wire.write(REQUEST_SET_MASTER);
  Wire.write(MASTER_ID);
  Wire.endTransmission();
  AVAILABLE_SLAVE_ADDRESSES[NUMBER_OF_AVAILABLE_SLAVES++] = slaveAddress;
  if (DEBUG){
    printAvailableSlaveAddresses();
  }
}



void printAvailableSlaveAddresses(){
  Serial.print("Number of available slaves is: ");
  Serial.print(NUMBER_OF_AVAILABLE_SLAVES);
  Serial.println(", they are:");
  for (int i = 0; i < NUMBER_OF_SLAVES; i++){
    Serial.println(AVAILABLE_SLAVE_ADDRESSES[i]);
  }
}



void endEvent(){
  releaseSlaves();
  Serial.println("--------KRAJ--------");
}

void releaseSlaves(){
  for (int i = 0; i < NUMBER_OF_SLAVES; i++){
    Wire.beginTransmission(AVAILABLE_SLAVE_ADDRESSES[i]);
    Wire.write(REQUEST_RESET_OWNERSHIP);
    Wire.endTransmission();
    if(DEBUG){
      Serial.print("OSLOBADJAM SLAVE-A: ");
      Serial.println(AVAILABLE_SLAVE_ADDRESSES[i]);
    }
    AVAILABLE_SLAVE_ADDRESSES[i] = 0;
  }
  NUMBER_OF_AVAILABLE_SLAVES = 0;
}


/*
 * Funkcija koja salje jedan paket slave-u
 * Kao argumente dobija paket koji treba da posalje i adresu slave-a
 */
void sendNextPackage(int slaveAddress){
  int i = 0;
  Package p = packages.pop();
  Wire.beginTransmission(slaveAddress);
  Wire.write(REQUEST_RECIEVE_PACKAGE);
  for (i = 0; i < 4; i++){
    Wire.write(p.x[i]);
  }
  
  for (i = 0; i < 4; i++){
    Wire.write(p.y[i]);
  }
  Wire.write(p.packageNum);
  Wire.endTransmission();
}

/*
 * Funkcija koja prihvata long od slave-a i vraca taj broj
 */
long recieveLong(){

  long l = 0;
  byte b1 = Wire.read();
  byte b2 = Wire.read();
  byte b3 = Wire.read();
  byte b4 = Wire.read();
  
  l = parseBytesToLong(b1, b2, b3, b4);

  return l;
  
}

/*
 * Funkcija koja prima jedan bajt od slave-a sa kojim je otvorena komunikacija
 */
byte recieveByte(){
  byte i = Wire.read();
  return i;
}





void checkOverflow(){
  if ((y - x) > 10000) {
    Serial.println("!!!PREVELIKI OPSEG BROJEVA!!!");
    Serial.end();
  }
}

/*
 * Funkcija koja generise pakete od unetog opsega
 */
void generatePackages(){
  long i = 0;
  long range = (y - x) / MIN_NUMBER_OF_PACKAGES;
 
  if (range > MAX_RANGE){
    range = MAX_RANGE;
  }
  if ((y - x) < MIN_NUMBER_OF_PACKAGES){
    range = 1;
  }
  
  NUMBER_OF_PACKAGES = (y - x) / range;
  
  
  if (DEBUG){
    Serial.print("range je: ");
    Serial.println(range);
    Serial.print("broj paketa je: ");
    Serial.println(NUMBER_OF_PACKAGES);
  }
  
  long localX = x;
  Package p;
  for (i = 0; i < NUMBER_OF_PACKAGES - 1; i++){
    parseLongsToPackage(localX, localX + range, &p);
    p.packageNum = i + 1;
    packages.push(p);
    localX = localX + range;
  }
    parseLongsToPackage(localX, localX + range, &p);
    p.packageNum = i + 1;
    packages.push(p);
}
/*
 * Funkcija koja printuje generisane pakete na serial
 */
void printPackages(){
  int i = 0;
  Package p;
  for (i = 0; i < NUMBER_OF_PACKAGES; i++){
    p = packages.pop();
    Serial.print("Paket: ");
    Serial.println(i + 1);
    Serial.print("X = ");
    Serial.println(parseBytesToX(&p));
    Serial.print("Byte 3 = ");
    Serial.println(p.x[0]);
    Serial.print("Byte 2 = ");
    Serial.println(p.x[1]);
    Serial.print("Byte 1 = ");
    Serial.println(p.x[2]);
    Serial.print("Byte 0 = ");
    Serial.println(p.x[3]);
    
    Serial.print("\nY = ");
    Serial.println(parseBytesToY(&p));
    Serial.print("Byte 3 = ");
    Serial.println(p.y[0]);
    Serial.print("Byte 2 = ");
    Serial.println(p.y[1]);
    Serial.print("Byte 1 = ");
    Serial.println(p.y[2]);
    Serial.print("Byte 0 = ");
    Serial.println(p.y[3]);

    Serial.println("-------------------------------");
    packages.push(p);
  }
  
}

//od paketa vraca long
long parseBytesToX(Package* p){

  long l = (((long)p->x[0] << 24) 
              + ((long)p->x[1] << 16) 
              + ((long)p->x[2] << 8) 
              + ((long)p->x[3] ) );

  return l;
}
long parseBytesToY(Package* p){

  long l = (((long)p->y[0] << 24) 
              + ((long)p->y[1] << 16) 
              + ((long)p->y[2] << 8) 
              + ((long)p->y[3] ) );

  return l;
}

long parseBytesToLong(byte b1, byte b2, byte b3, byte b4){


  

  long l = (((long)b1 << 24) 
              + ((long)b2 << 16) 
              + ((long)b3 << 8) 
              + ((long)b4 ) );

  return l;
}

//parsira dato x i y (a i b) u bajtove, i pakuje ih u zadati paket
void parseLongsToPackage(unsigned long a, unsigned long b, Package* p){

  p->x[0] = (byte)((a >> 24) & 0xFF);
  p->x[1] = (byte)((a >> 16) & 0xFF);
  p->x[2] = (byte)((a >> 8) & 0XFF);
  p->x[3] = (byte)((a & 0XFF));

  p->y[0] = (byte)((b >> 24) & 0xFF) ;
  p->y[1] = (byte)((b >> 16) & 0xFF) ;
  p->y[2] = (byte)((b >> 8) & 0XFF);
  p->y[3] = (byte)((b & 0XFF));

}








//------------------SERIAL FUNKCIJE------------------------

/*
 * Funkcija koja pretvara dati char u integer
 */
int charToInteger(char c){
  return c - 48;
}

/*
 * Funkcija koja preskace space na serial-u
 */
void skipSpaceSerial(){
  while (Serial.peek() == ' ' || isalpha(Serial.peek())){
    Serial.read();
    //Serial.println("Preskocio space");
  }
}

/*
 * Funkcija koja cita i parsira input iz seriala
 */
long parseInputFromSerial(){
  
  long a1 = 0;
  long a2 = 0;

  skipSpaceSerial();
  
  a1 = getLongFromSerial();
  
  skipSpaceSerial();

  a2 = getLongFromSerial();
  
  x = a1;
  y = a2;
}

/*
 * Funkcija koja dobavlja long iz seriala
 */
long getLongFromSerial(){
  long a1 = 0;
  char c = Serial.read();
  while (c != -1 && isdigit(Serial.peek())){
    a1 = a1*10 + charToInteger(c);
    //Serial.println(c);
    c = Serial.read();
  }
  a1 = a1*10 + charToInteger(c);
  return a1;
}

/*
 * Funkcija koja ispisuje broj slave-ova, X i Y
 */
void printStatus(){
  
  Serial.print("\nUneli ste X = ");
  Serial.print(x);
  Serial.print(", Y = ");
  Serial.println(y);
  Serial.print("Danas Vam je na usluzi ");
  Serial.print(NUMBER_OF_SLAVES);
  if (NUMBER_OF_SLAVES % 10 == 1){
    Serial.println(" rob");
  }
  else if ((NUMBER_OF_SLAVES % 10 < 6) && (NUMBER_OF_SLAVES % 10 > 1)){
    Serial.println(" roba");
  }
  else{
    Serial.println(" robova");
  }
  Serial.println("----------------------------");
  
}


void printWelcomeMessage(){
  Serial.println("DOBRODOSLI U ROBOVLASNICKI RACUNALAC PROSTIH BROJEVA!");
  Serial.println("UNESITE BROJEVE ZA RACUNANJE:    (potrebno je da opseg brojeva bude manji od 10000)");
}

void flushSerial(){
  while (Serial.available() > 0){
    Serial.read();
  }
}

