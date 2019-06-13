// Check I2C working:
// sudo i2cdetect -y 1
//      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
// 00:          -- -- -- -- -- -- -- -- -- -- -- -- -- 
// 10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
// 20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
// 30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
// 40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
// 50: 50 51 52 53 54 55 56 57 -- -- -- -- -- -- -- -- 
// 60: 60 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
// 70: -- -- -- -- -- -- -- --                         

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <bcm2835.h>

class command_line{
private:
  int Ncalls;
  int HelpLevel;
  int VerbosityLevel;
  int TestLevel;
  bool ShowVers;
  bool WriteFIFO;
  int CommandLineError;
  void ShowVersion(char * cmd){
    printf("This program [%s] reads data from Melexis MLX90621\n",cmd);
    printf("Build date %s, %s - dmc\n",__DATE__,__TIME__);
  }
  void ShowHelp(char * cmd){
    printf("This program reads data from Melexis MLX90621\n");
    printf("chip and calculates temperature readings.  Needs root\n");
    printf("permission to read from chip and write to fifo.\n");
    printf("Command line options listed below.\n");
    printf("%s -h          Show help\n",cmd);
    printf("%s -Version    Show version\n",cmd);
    printf("%s -v0         Verbosity level 0\n",cmd);
    printf("%s -v1         Verbosity level 1\n",cmd);
    printf("%s -v2         Verbosity level 2\n",cmd);
    printf("%s -wf         Write temperature data to fifo\n",cmd);
    printf("%s -t0         Test mode 0 (write test array to fifo)\n",cmd);
    printf("%s -t1         Test mode 1 (read array from fifo)\n",cmd);
  }
public:
  int GetHelpLevel(){
    return(HelpLevel);
  }
  int GetVerbosityLevel(){
    return(VerbosityLevel);
  }
  int GetTestLevel(){
    return(TestLevel);
  }
  bool GetShowVersion(){
    return(ShowVers);
  }
  bool GetWriteFIFO(){
    return(WriteFIFO);
  }
  int ParseCommandLine(int argc, char **argv){
    int i=0;
    if(Ncalls==0){ // Only check once
      HelpLevel=0;
      VerbosityLevel=0;
      TestLevel=0;
      ShowVers=false;
      WriteFIFO=false;
      CommandLineError=0;
      do{
        //      printf("i=%d %d [%s]\n",i,argc,argv[i]);
        if(i!=0){
          if (0==strcmp(argv[i],"-help")){
            HelpLevel=2;
          } else if (0==strcmp(argv[i],"-h")){
            HelpLevel=1;
          } else if (0==strcmp(argv[i],"-Version")) {
              ShowVers=true;
          } else if (0==strcmp(argv[i],"-v0")) {
              VerbosityLevel=1;
          } else if (0==strcmp(argv[i],"-v1")) {
            VerbosityLevel=2;
          } else if (0==strcmp(argv[i],"-v2")) {
            VerbosityLevel=3;
          } else if (0==strcmp(argv[i],"-wf")) {
            WriteFIFO=true;
          } else if (0==strcmp(argv[i],"-t0")) {
            TestLevel=1;
          } else if (0==strcmp(argv[i],"-t1")) {
            TestLevel=2;
          } else {
            CommandLineError++;
          }
        }
        i++;
      }while(i<argc);
      if((CommandLineError>0)|ShowVers)ShowVersion(argv[0]);
      if((CommandLineError>0)|(HelpLevel!=0))ShowHelp(argv[0]);
    }
    Ncalls++;
    return(-CommandLineError);
  }
  command_line():Ncalls(0){}
  ~command_line(){}
}cl;

class fifo_file{
 private:
#define MLX_FIFO "/var/run/mlx.sock"
 public:
  int OpenIO(){
    mkfifo(MLX_FIFO, 0666); // no checking yet
    return(0);              // 0 for success 
  }
  int CloseIO(){
    unlink(MLX_FIFO); // no checking yet
    return(0);        // 0 for success
  }
  int WriteIO(float * Temperatures, int ArraySize){
    int fd;
    fd = open(MLX_FIFO, O_WRONLY);
    write(fd, Temperatures, ArraySize*4);
    close(fd);
    return(0); // 0 for success
  }
  int WriteIO(int * Temperatures, int ArraySize){
    int fd;
    fd = open(MLX_FIFO, O_WRONLY);
    write(fd, Temperatures, ArraySize*4);
    close(fd);
    return(0); // 0 for success
  }
  int ReadIO(int * T, int ArraySize){
    int fd;
    fd = open(MLX_FIFO, O_RDONLY);
    read(fd, T, ArraySize*4);
    close(fd);
    return(0);
  }
  int ReadIO(float * T, int ArraySize){
    int fd;
    fd = open(MLX_FIFO, O_RDONLY);
    read(fd, T, ArraySize*4);
    close(fd);
    return(0);
  }
  fifo_file(){ }
  ~fifo_file(){ CloseIO(); }
}fifo;

class melexis{
private:
  int WriteReadI2C(int SlaveAddress,char* WriteArray,int WriteSize,char* ReadArray,int ReadSize){
    bcm2835_i2c_begin();
    bcm2835_i2c_setSlaveAddress(SlaveAddress);
    if(ReadSize==0){
      return( ( bcm2835_i2c_write(WriteArray, WriteSize)
                == BCM2835_I2C_REASON_OK )? 0 : -1 );
    } else {
      return( ( bcm2835_i2c_write_read_rs(WriteArray,WriteSize,ReadArray,ReadSize)
                == BCM2835_I2C_REASON_OK ) ? 0 : -1 );
    }    // return 0 is good
  }
#define EESIZE 256
  char eepromArray[EESIZE];
  int read_eeprom() {
    char read_eeprom_cmd[] = {0};
    return(WriteReadI2C(0x50,read_eeprom_cmd,1,eepromArray,EESIZE));
  }
  int SignedValEEprom(int HiByte,int LoByte){
    return(((signed char)eepromArray[HiByte] << 8 ) | eepromArray[LoByte]);
  }
  int SignedValEEprom(int LoByte){ return((signed char)eepromArray[LoByte]); }
  int LoNibbleEEprom(int Byte){ return(eepromArray[Byte]&0x0f); }
  int HiNibbleEEprom(int Byte){ return((eepromArray[Byte]&0xf0)>>4); }
  char configArray[2];
  int read_config() {
    char read_config_cmd[] = { 0x02, 0x92, 0x00, 0x01 };
    return(WriteReadI2C(0x60,read_config_cmd,4,configArray,2));
  }
  int get_config(){
    return(( configArray[1] << 8 ) | configArray[0]);
  }
  char trimArray[2];
  int read_trim() {
    char read_trim_cmd[] = { 0x02, 0x93, 0x00, 0x01 };
    return(WriteReadI2C(0x60,read_trim_cmd,4,trimArray,2));
  }
  int get_trim(){
    return(( trimArray[1] << 8 ) | trimArray[0]);
  }
  char ptatArray[2];
  int read_ptat() {
    char read_ptat_cmd[] = { 0x02, 0x40, 0x00, 0x01 };
    return(WriteReadI2C(0x60,read_ptat_cmd,4,ptatArray,2));
  }
  int get_ptat(){
    return(( ptatArray[1] << 8 ) | ptatArray[0]);
  }
  char vcompArray[2];
  int read_vcomp() {
    char read_vcomp_cmd[] = { 0x02, 0x41, 0x00, 0x01 };
    return(WriteReadI2C(0x60,read_vcomp_cmd,4,vcompArray,2));
  }
  int get_vcomp(){
    return( ((signed char)vcompArray[1] << 8 ) | vcompArray[0] );
  }
  char irArray[128];
  int read_ir() {
    char read_ir_cmd[] = { 0x02, 0x00, 0x01, 0x40 };
    return(WriteReadI2C(0x60,read_ir_cmd,4,irArray,128));
  }
  int get_ir(int index){
    int i;
    i=2*index;
    return((((signed char)irArray[i+1])<<8)|irArray[i]);
  }
  int write_config(char * Array ) {
    char write_config[] = { 0x03,(char)( Array[0]-0x55), Array[0],
                            (char)(Array[1]-0x55), Array[1] };
    return(WriteReadI2C(0x60,write_config,5,NULL,0));
  }
  int write_config(int x){
    char xx[] = { (char)(x & 0xff), (char)( (x>>8)&0xff )};
    return(write_config(xx));
  }
  int write_trim(char * Array) {
    char write_trim[] = { 0x04, (char)(Array[0]-0xAA), Array[0],
                          (char)( Array[1]-0xAA), Array[1] };
    return(WriteReadI2C(0x60,write_trim,5,NULL,0));
  }
  int write_trim(int x){
    char xx[] = { (char)(x & 0xff), (char)( (x>>8)&0xff )};
    return(write_trim(xx));
  }
  float Vth,Tamb;
  float Tak4;
  float Kt1,Kt2;
  float Acp,Bcp,Ccp,Vcomp,Sens;
  float TempDeg[64],Aij[64],Bij[64],Cij[64];
  int ExtractCoeff(){
    int MaxRes,Kt1scale,Kt2scale;
    int i,Ascale,Bscale;
    float Acommon,Cscale;
    MaxRes   =  3 - ( (get_config()&0x18)>>3 );
    Kt1scale = HiNibbleEEprom(0xD2);
    Kt2scale = LoNibbleEEprom(0xD2);
    Ascale   = HiNibbleEEprom(0xD9);
    Bscale   = LoNibbleEEprom(0xD9);
    Cscale   = 1/pow(2.0,eepromArray[0xE3]);
    
    Vth = ((float)SignedValEEprom(0xDB,0xDA))/pow(2.0,MaxRes);
    Kt1 = ((float)SignedValEEprom(0xDD,0xDC))/pow(2.0,Kt1scale+MaxRes);
    Kt2 = ((float)SignedValEEprom(0xDF,0xDE))/pow(2.0,10+Kt2scale+MaxRes);

    if(read_ptat()!=0)return(-1);
    // Got all information to calculate ambient temperature   
    Tamb  = sqrt( (Kt1*Kt1) - (Vth-(float)get_ptat()) * 4.0*Kt2 ) - Kt1;
    Tamb /= (2*Kt2);
    Tamb += 25;
    
    if(read_vcomp()!=0)return(-1);
    Tak4=pow(Tamb+273.15,4);
    Acp = ((float)SignedValEEprom(0xD4,0xD3)) / pow(2.0,MaxRes);
    Bcp = ((float)SignedValEEprom(0xD5)) / pow(2.0,Bscale+MaxRes);
    Ccp = ((float)SignedValEEprom(0xD7,0xD6)) / pow(2.0,eepromArray[0xE2]+MaxRes);
    Vcomp = (float)get_vcomp() - (Acp + (Tamb-25.0)*Bcp);
    Acommon = SignedValEEprom(0xD1,0xD0);
    Sens = ((float)SignedValEEprom(0xE1,0xE0)) / pow(2.0,eepromArray[0xE2]);
    
    for(i=0;i<64;i++){
      Aij[i]  = eepromArray[i];
      Aij[i]  = (Aij[i]*pow(2.0,Ascale) + Acommon) / pow(2.0,MaxRes);
      
      Bij[i]  = SignedValEEprom(i+0x40);
      Bij[i] /= pow(2.0, Bscale+MaxRes);

      Cij[i]  =  eepromArray[i+0x80];
      Cij[i]  = ( Cij[i]*Cscale + Sens) / pow(2.0,MaxRes);
    }
    return(0);
  }
  void ProcessTempArray(){
    int i;
    double t;// Does double or float make any difference?
    for(i=0;i<64;i++){
      t = get_ir(i);
      t  = (( t - ( Aij[i] + (Tamb-25.0)* Bij[i]) ) / Cij[i] ) + Tak4;
      TempDeg[i] = sqrt(sqrt(t)) - 273.15;
    }
  }

  void ShowArray(char * title,char * array, int size){
    int j;
    printf("%s :=\n    ",title);
    for(j=0;j<16;j++)printf("   %02x%c",j,(j==15)?'\n':' ');
    for(j=0;j<size;j++){
      if((j%16)==0)printf("%02x: ",j);
      printf("   %02x%c",array[j],(((j%16)==15)|(j==(size-1)))?'\n':' ');
    }
  }
    
  void ShowArray(char * title,float * array, int size){
    int j;
    printf("%s :=\n    ",title);
    for(j=0;j<16;j++)printf("   %02x%c",j,(j==15)?'\n':' ');
    for(j=0;j<size;j++){
      if((j%16)==0)printf("%02x: ",j);
      printf("%05.1f%c",array[j],(((j%16)==15)|(j==(size-1)))?'\n':' ');
    }
  }
  
public:
  void ShowCoeff(int Verbose){
    if(Verbose>=2){
      printf("Vth  = %.3e\n",Vth);
      printf("Tamb = %7.2f\n",Tamb);
      printf("Tak4 = %.3e\n",Tak4);
      printf("Kt1  = %7.2f\n",Kt1);
      printf("Kt2  = %7.2f\n",Kt2);
      printf("Acp  = %7.2f\n",Acp);
      printf("Bcp  = %7.2f\n",Bcp);
      printf("Ccp  = %.3e\n",Ccp);
      printf("Vcomp= %7.2f\n",Vcomp);
      printf("Sens = %.3e\n",Sens);
      if(Verbose>=3){
        char ArrayName[30];
        strcpy(ArrayName,"Tarray Aij");
        ShowArray(ArrayName,Aij,64);
        strcpy(ArrayName,"Tarray Bij");
        ShowArray(ArrayName,Bij,64);
        strcpy(ArrayName,"Tarray Cij");
        ShowArray(ArrayName,Cij,64);
      }
    }
  }
  void ShowEEPROM(){
    char ArrayName[]="EEprom";
    ShowArray(ArrayName,eepromArray,256);
  }
  float ReadTemp(){
    return(Tamb);
  }
  float ReadTemp(int i){
    return(TempDeg[i]);
  }
  int ShowTarray(int Verbose){
    char ArrayName[50];
    read_ir();
    ProcessTempArray();
    if(Verbose>=3){
      strcpy(ArrayName,"Tarray");
      ShowArray(ArrayName,irArray,128);
    }
    if(Verbose>=1){
      printf("Tamb %2.1f 'C  ",ReadTemp());
      strcpy(ArrayName,"Array temperature 'C");
      ShowArray(ArrayName,TempDeg,64);
    }
  }
  int Init(int Verbose) {
    if (bcm2835_init()){
      bcm2835_i2c_begin();
      bcm2835_i2c_set_baudrate(400000);
      usleep(5000); // 5ms
      if ( read_eeprom()==0 ) {
        if ( write_trim( eepromArray[0xF7] ) == 0 ) {
          if ( write_config( eepromArray[0xF5] + ( eepromArray[0xF6]<<8 ) ) == 0){
            // modify update rate ?
            if ( read_config() == 0){
              //printf("Config = %04x\n",get_config());
              ExtractCoeff();
              ShowCoeff(Verbose);
              return(0); // Successful read of eeprom and setting config
            }
          }
        }
      }
    }
    return(-1);
  }
  melexis(){
  }
  ~melexis(){
  }
}melexis;

void got_sigint(int sig) {
  bcm2835_i2c_end();
  fifo.CloseIO();
  exit(0);  
}


int main(int argc, char **argv){
  float  Ftemperature[64];
  float FTest[64];
  int i,j=0;
  signal(SIGINT, got_sigint);
  
  if((cl.ParseCommandLine(argc, argv)==0) &
     (cl.GetHelpLevel()==0) &
     !cl.GetShowVersion() ){
    
    if(cl.GetTestLevel()==1){ // Create float point test array and write to fifo
      fifo.OpenIO();
      do{
        printf("j=%d\n",j);
        for(i=0;i<64;i++)FTest[i]=i*2.3;
        FTest[j]=17;
        fifo.WriteIO(FTest,64);
        if(j<63){
          j++;
        } else {
        j=0;
        }
        sleep(1);
      } while(1);
    } else if(cl.GetTestLevel()==2){ // read and display numbers from fifo
      fifo.OpenIO();
      for(i=0;i<64;i++)FTest[i]=0;
      do{
        fifo.ReadIO(FTest,64);
        for(i=0;i<64;i++)printf("%5.1f %s",FTest[i],((i%16)==15)?"\n":"   ");
      } while(1);
    } else {      // readand display temperature
      if(cl.GetWriteFIFO())fifo.OpenIO();
      if(melexis.Init(cl.GetVerbosityLevel())==0){
        do{
          while(melexis.CheckBrownOut()!=1){
            sleep(1);
            printf("Init = %d\n",melexis.Init(cl.GetVerbosityLevel()));
            if(cl.GetVerbosityLevel()!=0)melexis.ShowEEPROM();
          }
          melexis.ShowTarray(cl.GetVerbosityLevel());
          for(i=0;i<64;i++)Ftemperature[i]=melexis.ReadTemp(i);
          if(cl.GetWriteFIFO()) fifo.WriteIO(Ftemperature,64);
          sleep(1);
        } while(1);
      }
    }
  }
}    


