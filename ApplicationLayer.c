#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "DataLink.h"
#include "defines.h"
#include "termiosManip.h"
#include "LinkLayer.h"
#include "File.h"
#include "ApplicationLayer.h"
#include "libgen.h"

LinkLayer* Llayer;

int receiveFile(){
  File* file = (File*)malloc(sizeof(File));
  int fd = llopen(RECEIVER);

  receiveControl(fd,CONTROL_START,file);

  file->fileStream = fopen(file->fileName,"wb");

  int seq = 0;

  receiveData(fd,seq,file);

  llclose(fd,RECEIVER);
}


int sendFile(){

  File* file = loadFile();
  int fd = llopen(TRANSMITTER);

  sendControl(fd,CONTROL_START,file);

  int seq = 0;

  sendData(fd,seq,file->fileSize,0,file);

  llclose(fd,TRANSMITTER);
}

int sendData(int fd,int seq,int nbyte,int foffset,File* file){
  unsigned char buf[MAX_SIZE*4];
  int i = 0;

  fseek(file->fileStream,foffset,SEEK_SET);
  unsigned char* tempBuf = (unsigned char*) malloc(nbyte);
  int nread = fread(tempBuf,sizeof(unsigned char),nbyte,file->fileStream);
  fseek(file->fileStream,foffset,SEEK_SET);

  buf[i++] = CONTROL_DATA;
  buf[i++] = (unsigned char)seq;
  int l1 = nread % 256;
  int l2 = nread / 256;
  buf[i++] = (unsigned char)l2;
  buf[i++] = (unsigned char)l1;
  memcpy(buf+i,tempBuf,nread);
  i+= nread;
  free(tempBuf);

  int sent = llwrite(fd,buf,i);

  if(sent < 0)
    return FAILURE;
  return SUCCESS;
}

int receiveData(int fd,int seq,File* file){
  unsigned char buf[MAX_SIZE*4];
  int messageSize = llread(fd,buf);
  if(messageSize < 0)
    return FAILURE;

  int i = 0;

  if (buf[i++] != CONTROL_DATA){
    return FAILURE;
  }
  //Verifica se o pacote recebido é o proximo item da sequencia
  if(buf[i++] != seq){
    return FAILURE;
  }
  int l2 = (int)buf[i++];
  int l1 = (int)buf[i++];
  int nbyte = l1+256*l2;
  unsigned char* tempBuf = (unsigned char*)malloc(nbyte);
  memcpy(tempBuf,buf+i,nbyte);

  fwrite(tempBuf,sizeof(unsigned char),nbyte,file->fileStream);

}



File * loadFile(){
  File* file;
  unsigned char * fileName = (unsigned char*)malloc(MAX_SIZE);
  printf("Insert file name: ");
  scanf("%s",fileName);
  file = initFile(fileName,"rb");
  if(file == NULL){
    printf("Error opening file \"%s\", exiting...\n",fileName);
    exit(1);
  }else printf("File loaded with success.\n");

  return file;
}

int sendControl(int fd,int c,File* file){

  if(c != CONTROL_START && c != CONTROL_END){
    return FAILURE;
  }
  int i = 0;
  //Buffer da mensagem a enviar
  unsigned char buf[MAX_SIZE];
  buf[i++] = c;

  //Colocaçao do TYPE de  tamanho do ficheiro
  buf[i++] = CONTROL_TYPE_SIZE;
  //Coloca tamanho do ficheiro num buffer de unsigned chars(cada unsigned char e um numero)
  unsigned char sizeInArray[20];
  sprintf(sizeInArray,"%ld",file->fileSize);
  //Colocaçao do Length de tamanho de ficheiro
  buf[i++] = strlen(sizeInArray);
  int j;
  //Colocaçao do tamanho de ficheiro
  for(j = 0;j < strlen(sizeInArray);j++){
    buf[i++] = sizeInArray[j];
  }

  //Colocaçao do TYPE de nome de ficheiro
  buf[i++] = CONTROL_TYPE_NAME;
  //Coloca nome de ficheiro num buffer
  unsigned char* fileNameWithoutDir = basename(file->fileName);
  //Colocaçao de LENGTH do nome de ficheiro
  buf[i++] = (unsigned char)strlen(fileNameWithoutDir);
  //Colocaçao do nome de ficheiro
  for(j = 0;j < strlen(fileNameWithoutDir);j++){
    buf[i++] = fileNameWithoutDir[j];
  }

  if(c == CONTROL_START)
    printf("Sending file \"%s\"(%ld bytes)\n",fileNameWithoutDir,file->fileSize);

  if(c == CONTROL_END)
    printf("Finished sending file \"%s\"(%ld bytes)\n",fileNameWithoutDir,file->fileSize);

  int sent = llwrite(fd,buf,i);
  if(sent < 0)
    return FAILURE;
  return SUCCESS;
}

int receiveControl(int fd, int c,File* file){
  unsigned char buf[MAX_SIZE];
  int messageSize = llread(fd,buf);
  if(messageSize < 0)
    return FAILURE;

  int i = 0;

  if(buf[i++] != (unsigned char)c)
    return FAILURE;
  //Cada ciclo le um parametro TLV
  while(i < messageSize){
    int currType = (int)buf[i++];
    printf("%d\n",currType);
    if(currType == CONTROL_TYPE_SIZE){
      int length = (int)buf[i++];
      unsigned char* value = (unsigned char*)malloc(length+1);
      memcpy(value,buf+i,length);
      value[length] = 0;
      long int size = -1;
      sscanf(value,"%ld",&size);
      free(value);
      file->fileSize = size;
      i+= length;
    }
    else if(currType == CONTROL_TYPE_NAME){
      int length = (int)buf[i++];
      unsigned char* value = (unsigned char*)malloc(length);
      memcpy(value,buf+i,length);
      file->fileName = value;
      i+= length;
    }
  }
  if(c == CONTROL_START)
    printf("Receiving file \"%s\"(%ld bytes)\n",file->fileName,file->fileSize);

  if(c == CONTROL_END)
    printf("Finished receiving file \"%s\"(%ld bytes)\n",file->fileName,file->fileSize);

    return SUCCESS;

}



int main(int argc, unsigned char** argv){
  signal(SIGALRM, atende_alarm);

	if ( (argc != 3) ||
	((strcmp("/dev/ttyS0", argv[1])!=0) &&
	(strcmp("/dev/ttyS1", argv[1])!=0) )) {
		printf("Usage:\tnserial SerialPort MODE\n\tex: nserial /dev/ttyS1 TRANSMITTER\n");
		exit(1);
	}

	int transorres = -1;

	if(strcmp("TRANSMITTER",argv[2]) == 0){
		transorres = TRANSMITTER;
	}else
	if(strcmp("RECEIVER",argv[2]) == 0){
		transorres = RECEIVER;
	}else{
		printf("Third argument has to be TRANSMITTER or RECEIVER");
		exit(1);
	}
  Llayer = createLinkLayer(argv[1], BAUDRATE, 0, 3, 3);
  if(transorres == TRANSMITTER){
		sendFile();
	}else{
		receiveFile();
	}
}
