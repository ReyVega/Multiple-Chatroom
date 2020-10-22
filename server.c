#include<string.h>
#include<sys/socket.h>
#include<stdio.h>
#include<sys/types.h>
#include<signal.h>
#include<stdlib.h>
#include<unistd.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<errno.h>
#include<pthread.h>


#define BUFFER_SZ 2048
#define CLIENTES_TOPE 5

static int uid = 10;
static _Atomic unsigned int contadorClientes = 0;

// Estructura del cliente
typedef struct{
	struct sockaddr_in direccion;
	int sockfd;
	int uid;
	char nombre[32];
} clientsStructure;

clientsStructure *clientes[CLIENTES_TOPE];

pthread_mutex_t clientes_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

void almacenarMsjONombre(char* array, int length) {
	for (int i = 0; i < length; i++) { 
		if (array[i] == '\n') {
    		array[i] = '\0';
      		break;
    	}
  	}
}

void imprimirDireccionCliente(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

// Añadir clientes a la cola
void anadirCola(clientsStructure *cl){
	pthread_mutex_lock(&clientes_mutex);

	for(int i = 0; i < CLIENTES_TOPE; i++){
		if(!clientes[i]){
			clientes[i] = cl;
			break;
		}
	}
	pthread_mutex_unlock(&clientes_mutex);
}

// Quitar clientes de la cola
void removerCola(int uid){
	pthread_mutex_lock(&clientes_mutex);

	for(int i = 0; i < CLIENTES_TOPE; i++){
		if(clientes[i]){
			if(clientes[i]->uid == uid){
				clientes[i] = NULL;
				break;
			}
		}
	}
	pthread_mutex_unlock(&clientes_mutex);
}

// Enviar mensaje a todos menos al que lo envió
void enviarMensaje(char *mensaje, int uid){
	pthread_mutex_lock(&clientes_mutex);
	for(int i = 0; i < CLIENTES_TOPE; i++){
		if(clientes[i]){
			if(clientes[i]->uid != uid){
				if(write(clientes[i]->sockfd, mensaje, strlen(mensaje)) < 0){
					perror("ERROR");
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&clientes_mutex);
}

// Manejar las comunicaciones del cliente
void *manejadorCliente(void *arg){
	char buff_out[BUFFER_SZ];
	char nombre[32];
	int banderaSalida = 0;

	contadorClientes++;
	clientsStructure *cli = (clientsStructure *)arg;

	// Manejar nombre
	if(recv(cli->sockfd, nombre, 32, 0) <= 0 || strlen(nombre) <  2 || strlen(nombre) >= 32-1){
		printf("No se ingreso nombre o excedio el limite.\n");
		banderaSalida = 1;
	} else{
		strcpy(cli->nombre, nombre);
		printf("> ");
		sprintf(buff_out, "%s conectado\n", cli->nombre);
		printf("%s", buff_out);
		enviarMensaje(buff_out, cli->uid);
	}

	bzero(buff_out, BUFFER_SZ);

	while(1){
		if (banderaSalida) {
			break;
		}

		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0){
			if(strlen(buff_out) > 0){
				enviarMensaje(buff_out, cli->uid);
				almacenarMsjONombre(buff_out, strlen(buff_out));
				printf("> ");
				printf("%s\n", buff_out, cli->nombre);
			}
		} else if (receive == 0 || strcmp(buff_out, "bye") == 0 || strcmp(buff_out, "exit") == 0){
			printf("> ");
			sprintf(buff_out, "%s desconectado\n", cli->nombre);
			printf("%s", buff_out);
			enviarMensaje(buff_out, cli->uid);
			banderaSalida = 1;
		} else {
			printf("Solo puede escribirs msjs de 250 caracteres:\n");
			banderaSalida = 1;
		}
		bzero(buff_out, BUFFER_SZ);
	}

    // Borrar cliente del arrayeglo y mejorar el rendimiento del hilo
	close(cli->sockfd);
  	removerCola(cli->uid);
  	free(cli);
  	contadorClientes--;
  	pthread_detach(pthread_self());
	return NULL;
}

int main(int argc, char **argv){
	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);
	int option = 1;
	int listenfd = 0, connfd = 0;
  	struct sockaddr_in serv_addr;
  	struct sockaddr_in cli_addr;
  	pthread_t tid;

  	// Configuración del socket
  	listenfd = socket(AF_INET, SOCK_STREAM, 0);
  	serv_addr.sin_family = AF_INET;
  	serv_addr.sin_addr.s_addr = inet_addr(ip);
  	serv_addr.sin_port = htons(port);

  	// Ignorar señales pipe
	signal(SIGPIPE, SIG_IGN);

	if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERROR");
    	return EXIT_FAILURE;
	}

	// Verificar que se puede hacer bind
  	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    	perror("ERROR");
    	return EXIT_FAILURE;
  	}

  	// Manejar listen en caso de fallo
  	if (listen(listenfd, 10) < 0) {
    	perror("ERROR");
    	return EXIT_FAILURE;
	}

	printf("Servidor inicializado\n");

	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		// Checar si el numero de clientes se alcanzó
		if((contadorClientes + 1) == CLIENTES_TOPE){
			printf("Tope de clientes alcanzado");
			imprimirDireccionCliente(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}

		// Configuración del cliente
		clientsStructure *cli = (clientsStructure *)malloc(sizeof(clientsStructure));
		cli->direccion = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;

		// Añadir cliente a la cola y hacer fork del hilo
		anadirCola(cli);
		pthread_create(&tid, NULL, &manejadorCliente, (void*)cli);

		// Reducir el uso del CPU
		sleep(1);
	}
	return EXIT_SUCCESS;
}