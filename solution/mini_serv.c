/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   mini_serv.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: anonymous <anonymous@student.42.fr>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/27 13:33:34 by mbolano-          #+#    #+#             */
/*   Updated: 2025/12/04 06:27:33 by anonymous        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <errno.h> // Biblioteca para manejo de errores (errno). No se usa directamente en este código, pero es buena práctica incluirla al trabajar con funciones del sistema que pueden fallar.
#include <netdb.h> // Biblioteca para manejo de bases de datos de red. No se usa directamente en este código, pero es común incluirla en programas de red.
#include <string.h> // Funcionas para manejo de cadenas: strlen(), strcpy(), strcat() y bzero().
#include <unistd.h> // Funciones del sistema: write(), close() y read().
#include <sys/socket.h> // Funciones: socket(), bind(), listen(), accept(), recv(), send(); tipo de datos: socklen_t; y estructuras: struct sockaddr [las funciones bind() y accept() esperan un putero a "struct sockaddr *", por lo que se ha de castear la variable: "(struct sockaddr *)&servaddr"]; para manejo de sockets.
#include <sys/select.h> // Funciones y macros para manejo de select(): select(), FD_SET, FD_CLR, FD_ISSET, FD_ZERO.
#include <netinet/in.h> // Estructuras: struct sockaddr_in; constantes: AF_INET; y funciones: htons(), htonl(); para manejo de direcciones de red.
#include <stdlib.h> // Funciones de gestión de memoria dinámica: malloc(), free(), calloc(), exit(); y conversión de cadenas: atoi().
#include <stdio.h> // Funciones de entrada/salida: sprintf().

typedef struct	s_client {
	int		id;			// Identificador único del cliente.
	char	*buffer;	// Buffer para almacenar datos parciales recibidos del cliente. Maneja mensajes fragmentados o múltiples mensajes en un solo recv().
}	t_client;

// Variables globales:
t_client	clients[65536]; // El valor 65536 se usar porque es el número máximo de "file descriptors" posibles en un sistema UNIX/Linux.
int			next_id = 0;
int			max_fd = 0;
int			sockfd;
fd_set		read_fds, active_fds;

// Función para extraer un mensaje completo (terminado en '\n') del buffer del cliente.
// Esta función viene incluida en el "main.c" proporcionado por el enunciado.

int	extract_message(char **buf, char **msg)
{
	// Nueva variable para almacenar el nuevo buffer después de extraer el mensaje:
	char *newbuf;
	int i;

	// Inicializamos *msg a NULL:
	*msg = NULL;
	// Si el buffer es NULL, no hay nada que extraer:
	if (*buf == NULL)
		return (0);
	i = 0;
	// Recorremos el buffer buscando el carácter de nueva línea ('\n'):
	while ((*buf)[i])
	{
		// Encontramos el carácter de nueva línea:
		if ((*buf)[i] == '\n')
		{
			// Reservamos memoria para el nuevo buffer e inicializamos a cero:
			newbuf = calloc(strlen(*buf + i + 1) + 1, sizeof(char)); // strlen(*buf + i + 1) calcula la longitud del contenido restante después del mensaje extraído.
			// Si no se puede asignar memoria para el nuevo buffer, devolvemos -1 indicando un error.
			if (newbuf == NULL)
				return (-1);
			// Copiamos el contenido restante del buffer original (después del mensaje extraído: "*buf + i + 1") al nuevo buffer:
			strcpy(newbuf, *buf + i + 1);
			// Asignamos el mensaje extraído a *msg:
			*msg = *buf;
			// Terminamos el mensaje en la posición del carácter de nueva línea:
			(*msg)[i + 1] = '\0';
			// Actualizamos el buffer original para que apunte al nuevo buffer:
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

// Función para concatenar dos cadenas, liberando la primera si es necesario.
// Esta función viene incluida en el "main.c" proporcionado por el enunciado.
// No obstante, se ha modificado para mejorar su legibilidad.

char	*str_join(char *buf, char *add)
{
	char *newbuf;
	int len;

	if (buf == NULL)
		len = 0;
	else
		len = strlen(buf);
	newbuf = calloc(len + strlen(add) + 1, sizeof(char));
	if (newbuf == NULL)
		return (NULL);
	if (buf != NULL)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void	fatal_error(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void send_to_all(int sender_fd, char *msg)
{
    for (int fd = 0; fd <= max_fd; fd++)
    {
        if (FD_ISSET(fd, &active_fds) && fd != sender_fd && fd != sockfd)
        {
            send(fd, msg, strlen(msg), 0);
        }
    }
}

void remove_client(int fd)
{
    char msg[64];
    
    sprintf(msg, "server: client %d just left\n", clients[fd].id);
    send_to_all(fd, msg);
    
    FD_CLR(fd, &active_fds);
    if (clients[fd].buffer)
        free(clients[fd].buffer);
    clients[fd].buffer = NULL;
    clients[fd].id = -1;
    close(fd);
}

void add_client(int connfd)
{
    clients[connfd].id = next_id++;
    clients[connfd].buffer = NULL;
    
    if (connfd > max_fd)
        max_fd = connfd;
    
    FD_SET(connfd, &active_fds);
    
    char msg[64];
    sprintf(msg, "server: client %d just arrived\n", clients[connfd].id);
    send_to_all(connfd, msg);
}

void handle_client_data(int fd)
{
    char buffer[65536];
    char *msg;
    int ret;
    
    ret = recv(fd, buffer, sizeof(buffer) - 1, 0);
    
    if (ret <= 0)
    {
        remove_client(fd);
    }
    else
    {
        buffer[ret] = '\0';
        clients[fd].buffer = str_join(clients[fd].buffer, buffer);
        
        if (clients[fd].buffer == NULL)
            fatal_error();
        
        while (extract_message(&clients[fd].buffer, &msg) == 1)
        {
            char *full_msg;
            int len;
            
            len = strlen(msg) + 32;
            full_msg = malloc(len);
            if (!full_msg)
                fatal_error();
            
            sprintf(full_msg, "client %d: %s", clients[fd].id, msg);
            send_to_all(fd, full_msg);
            free(full_msg);
            free(msg);
        }
    }
}

int	main(int argc, char **argv)
{
	int					connfd;
	socklen_t			len;
	// La estructura sockaddr_in está definida en <netinet/in.h> y se utiliza para manejar direcciones IP y puertos en sockets de red.
	//	struct sockaddr_in {
	//		short            sin_family;	<- Familia de direcciones (AF_INET para IPv4)
	//		unsigned short   sin_port;		<- Puerto (en formato de red)
	//		struct in_addr   sin_addr;		<- Dirección IP (en formato de red)
	//		char             sin_zero[8];	<- Relleno para igualar tamaño con sockaddr
	//	};
	struct sockaddr_in 	servaddr, cli;
	// Tal y como se pide en el enunciado, el servidor solo acepta un argumento: el puerto.
	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}
	
	// A continuación, almacenamos el file descriptor del socket del servidor en la variable global "sockfd":
	// - AF_INET (Address Family Internet): Protocolo IPv4.
	// - SOCK_STREAM: Tipo de socket orientado a conexión (TCP).
	// - 0: Protocolo por defecto (TCP para SOCK_STREAM).
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	// Comprobamos si la creación del socket fue exitosa:
	if (sockfd == -1)
		fatal_error();
	// Inicializamos max_fd con el valor del socket del servidor, porque es el primer file descriptor activo. A medida que se acepten nuevas conexiones, max_fd se actualizará con el valor del file descriptor más alto en uso:
	max_fd = sockfd;
	// La siguiente función inicializa a "ZERO" el conjunto de descriptores de archivo activos (active_fds):
	FD_ZERO(&active_fds);
	// Y, a continuación, añadimos el socket del servidor al conjunto de descriptores de archivo activos:
	FD_SET(sockfd, &active_fds);
	// OJO: sockfd, max_fd y active_fds son VARIABLES GLOBALES.
	// Ahora, configuramos la estructura servaddr para enlazar el socket del servidor a la dirección IP y puerto especificados:
	//	- bzero: Inicializa la estructura a cero.
	bzero(&servaddr, sizeof(servaddr));
	//	- sin_family: Familia de direcciones (AF_INET para IPv4).
	servaddr.sin_family = AF_INET;
	//	- sin_addr.s_addr: Dirección IP en formato de red (localhost en este caso).
	servaddr.sin_addr.s_addr = htonl(2130706433); // htonl convierte la dirección IP de formato host a formato de red. 2130706433 es la representación numérica de 127.0.0.1 (localhost)
	//	- sin_port: Puerto en formato de red (convertido de cadena a entero).
	servaddr.sin_port = htons(atoi(argv[1])); // htons convierte el puerto de formato host a formato de red. atoi convierte la cadena del argumento a un entero.
	// Enlazamos el socket del servidor a la dirección y puerto especificados en servaddr:
	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) // Si el puerto proporcionado como argumento fuera erróneo, bind() devolvería un valor distinto de cero y terminaría el programa.
		fatal_error();
	// Por último, ponemos el socket del servidor en modo escucha para aceptar conexiones entrantes:
	if (listen(sockfd, 128) != 0) // El valor 128 es el tamaño máximo (predeterminado) de la cola de conexiones pendientes. Se puede aumentar en Linux, por ejemplo, a 1024: "sudo sysctl -w net.core.somaxconn=1024"
		fatal_error();
	// Inicialización de la estructura de cliente ahora, porque puede haber ocurrido algún error que haya provocado la terminación del programa antes de entrar en el bucle principal:
	for (int i = 0; i < 65536; i++)
	{
		clients[i].id = -1;
		clients[i].buffer = NULL;
	}
    while (1)
    {
        read_fds = active_fds;
        
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
            continue;
        
        for (int fd = 0; fd <= max_fd; fd++)
        {
            if (FD_ISSET(fd, &read_fds))
            {
                if (fd == sockfd)
                {
                    len = sizeof(cli);
                    connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
                    if (connfd >= 0)
                        add_client(connfd);
                }
                else
                {
                    handle_client_data(fd);
                }
            }
        }
    }
    
    return 0;
}
