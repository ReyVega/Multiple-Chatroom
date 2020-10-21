#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<signal.h>
#include<pthread.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#define LENGTH 250

// Variable globales
int sockfd = 0;
char nombre[32];
volatile sig_atomic_t bandera = 0;

void iniciarChat() {
	printf("%s", "> ");
	fflush(stdout);
}

void almacenarMensajeONombre (char* array, int length) {
	for (int i = 0; i < length; i++) {
    	if (array[i] == '\n') {
      		array[i] = '\0';
      		break;
    	}
  	}
}

void catchCtrlCyExit(int signal) {
    bandera = 1;
}

void manejadorDeEnvioMsjs() {
  	char mensaje[LENGTH] = {};
	char buffer[LENGTH + 32] = {};

  	while(1) {
  		iniciarChat();
    	fgets(mensaje, LENGTH, stdin);
    	almacenarMensajeONombre(mensaje, LENGTH);

    	if(strcmp(mensaje, "exit") == 0 || strcmp(mensaje, "bye") == 0) {
			break;
    	} else {
      		sprintf(buffer, "%s: %s\n", nombre, mensaje);
      		send(sockfd, buffer, strlen(buffer), 0);
    	}
		bzero(mensaje, LENGTH);
    	bzero(buffer, LENGTH + 32);
  	}
	catchCtrlCyExit(2);
}

void manejadorReciboMsjs() {
	char mensaje[LENGTH] = {};
  	while (1) {
		int receive = recv(sockfd, mensaje, LENGTH, 0);
    	if (receive > 0) {
    		printf("%s", mensaje);
    		iniciarChat();
    	} else if (receive == 0) {
			break;
    	} 
		memset(mensaje, 0, sizeof(mensaje));
  	}
}

int main(int argc, char **argv){
	char *ip = argv[1];
	int port = atoi(argv[2]);

	signal(SIGINT, catchCtrlCyExit);

	strcat(argv[3],"\n");
	strcpy(nombre,argv[3]);
  	almacenarMensajeONombre(nombre, strlen(nombre));

	if (strlen(nombre) > 32 || strlen(nombre) < 2){
		printf("ERROR: ingresar nombre de 2 a 32 caracteres\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_addr;

	// Configuracion de sockets
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
  	server_addr.sin_family = AF_INET;
  	server_addr.sin_addr.s_addr = inet_addr(ip);
  	server_addr.sin_port = htons(port);

  	// Establecer conexion con el servidor
  	int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  	if (err == -1) {
		printf("Error al conectar\n");
		return EXIT_FAILURE;
	}

	// Enviar nombre
	send(sockfd, nombre, 32, 0);

	printf("Bienvenido %s\n", nombre);

	pthread_t hiloEnviarMsjs;
  	if(pthread_create(&hiloEnviarMsjs, NULL, (void *) manejadorDeEnvioMsjs, NULL) != 0) {
		printf("ERROR\n");
    	return EXIT_FAILURE;
	}

	pthread_t hiloRecibirMsjs;
  	if(pthread_create(&hiloRecibirMsjs, NULL, (void *) manejadorReciboMsjs, NULL) != 0) {
		printf("ERROR\n");
		return EXIT_FAILURE;
	}

	while (1) {
		if(bandera) {
			break;
    	}
	}
	close(sockfd);
	return EXIT_SUCCESS;
}