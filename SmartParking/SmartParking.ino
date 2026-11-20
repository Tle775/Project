// Classic ESP32-WROOM, four active-low IR modules and one active-low gate beam.
// USB serial protocol at 115200. Beam must be crossed and clear to close.
#include <ESP32Servo.h>
#include <Preferences.h>
#include <string.h>
Servo arm;
Preferences nv;
const int pins[] = {18,19,21,22,27}; // slots 1-4, then gate beam
const int servoPin = 23;
bool raw[5], stable[5];
unsigned long changed[5], lastReport, clearSince;
char job[33] = "", completed[33] = "";
bool crossed, ready;
void report() {
  Serial.print("STATE ");
  for(int i=0;i<5;i++) Serial.print(stable[i] ? '1':'0');
  Serial.print(' '); Serial.print(job[0]?job:"-");
  Serial.print(' '); Serial.println(completed[0]?completed:"-");
  lastReport=millis();
}
void command(char *line) {
  if(strncmp(line,"OPEN ",5)!=0 || strlen(line+5)!=32 ||
     strspn(line+5,"0123456789abcdef")!=32 || !ready) return;
  if(job[0] || strcmp(line+5,completed)==0) { report(); return; }
  if(nv.putString("job",line+5)==0) return;
  strcpy(job,line+5);
  crossed=false;
  arm.write(90);
  report();
}
void setup() {
  Serial.begin(115200);
  for(int i=0;i<5;i++){
    pinMode(pins[i],INPUT_PULLUP);
    raw[i]=stable[i]=digitalRead(pins[i])==LOW;
  }
  ready=nv.begin("parking",false);
  nv.getString("job","").toCharArray(job,sizeof(job));
  nv.getString("done","").toCharArray(completed,sizeof(completed));
  if(job[0] && strcmp(job,completed)==0 && nv.remove("job")) job[0]=0;
  arm.attach(servoPin);
  arm.write(job[0] || stable[4] || !ready ? 90 : 0);
}
void loop() {
  unsigned long now=millis();
  for(int i=0;i<5;i++){
    bool value=digitalRead(pins[i])==LOW;
    if(raw[i]!=value){raw[i]=value;changed[i]=now;}
    if(now-changed[i]>=100) stable[i]=value;
  }
  if(job[0]){
    if(stable[4]) { crossed=true;clearSince=now; }
    if(crossed && !raw[4] && !stable[4] && now-clearSince>=1200){
      if(nv.putString("done",job)>0 && nv.remove("job")){
        strcpy(completed,job);job[0]=0;crossed=false;
        arm.write(0);report();
      }
    }
  } else if(ready && !raw[4] && !stable[4]) {
    arm.write(0);
  }
  static char line[48];
  static int len=0;
  while(Serial.available()){
    char c=Serial.read();
    if(c=='\n'){if(len<(int)sizeof(line)){line[len]=0;command(line);}len=0;}
    else if(c!='\r' && len<(int)sizeof(line)) line[len++]=c;
  }
  if(ready && now-lastReport>=200) report();
}
