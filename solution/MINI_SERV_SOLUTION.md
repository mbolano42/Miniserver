# Mini Server - Solución del Ejercicio

## Descripción del Problema

El ejercicio `mini_serv` requiere implementar un **servidor de chat simple** que:

1. **Escucha conexiones** en un puerto específico (127.0.0.1)
2. **Maneja múltiples clientes** de forma concurrente y no-bloqueante
3. **Asigna IDs únicos** a cada cliente (0, 1, 2, ...)
4. **Notifica conexiones/desconexiones** a todos los clientes conectados
5. **Retransmite mensajes** entre clientes con formato específico

## Análisis del Código Inicial

El archivo `main.c` proporcionado contiene:
- **Funciones auxiliares**: `extract_message()` y `str_join()`
- **Inicio básico** de servidor TCP (socket, bind, listen, accept)
- **⚠️ Problema**: Solo acepta un cliente y no implementa la lógica completa

## Solución Paso a Paso

### Paso 1: Estructura de Datos
Necesitamos mantener información de múltiples clientes usando el fd como índice:
```c
typedef struct s_client {
    int id;           // ID único del cliente
    char *buffer;     // Buffer para mensajes parciales
} t_client;

t_client clients[65536];  // Array indexado por fd
int next_id = 0;
int max_fd = 0;
int sockfd;
fd_set read_fds, active_fds;  // Variables globales para select
```

### Paso 2: Validación de Argumentos
```c
if (argc != 2) {
    write(2, "Wrong number of arguments\n", 26);
    exit(1);
}
int port = atoi(argv[1]);
```

### Paso 3: Configuración del Servidor
- Crear socket con `socket(AF_INET, SOCK_STREAM, 0)`
- Configurar para **127.0.0.1** y puerto dado
- `bind()` y `listen()`
- Manejar errores con `"Fatal error\n"`

### Paso 4: Loop Principal con select()
```c
while (1) {
    // 1. Preparar fd_set con servidor + todos los clientes
    // 2. select() - esperar actividad
    // 3. Nueva conexión si actividad en socket servidor
    // 4. Datos de cliente si actividad en fd de cliente
    // 5. Manejo de desconexiones
}
```

### Paso 5: Manejo de Nuevas Conexiones
```c
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
```

### Paso 6: Procesamiento de Mensajes
```c
void handle_client_data(int fd)
{
    char buffer[65536];
    char *msg;
    int ret;
    
    ret = recv(fd, buffer, sizeof(buffer) - 1, 0);
    
    if (ret <= 0)
        remove_client(fd);
    else
    {
        buffer[ret] = '\0';
        clients[fd].buffer = str_join(clients[fd].buffer, buffer);
        
        while (extract_message(&clients[fd].buffer, &msg) == 1)
        {
            char *full_msg = malloc(strlen(msg) + 32);
            sprintf(full_msg, "client %d: %s", clients[fd].id, msg);
            send_to_all(fd, full_msg);
            free(full_msg);
            free(msg);
        }
    }
}
```

### Paso 7: Manejo de Desconexiones
```c
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
```

## Funciones Auxiliares (Análisis)

### `extract_message(char **buf, char **msg)`
- **Propósito**: Extrae mensajes completos terminados en `'\n'`
- **Retorno**: `1` (mensaje encontrado), `0` (no completo), `-1` (error)
- **Uso**: Procesar múltiples líneas en un solo `recv()`

### `str_join(char *buf, char *add)`
- **Propósito**: Concatena strings liberando buffer anterior
- **Uso**: Acumular datos parciales en buffer del cliente

## Implementación Completa

Ahora voy a mostrar cómo implementar la solución completa:

```c
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct s_client {
    int id;
    char *buffer;
} t_client;

t_client clients[65536];
int next_id = 0;
int max_fd = 0;
int sockfd;
fd_set read_fds, active_fds;

void fatal_error(void)
{
    write(2, "Fatal error\n", 12);
    exit(1);
}

int extract_message(char **buf, char **msg)
{
    char *newbuf;
    int i;

    *msg = 0;
    if (*buf == 0)
        return (0);
    i = 0;
    while ((*buf)[i])
    {
        if ((*buf)[i] == '\n')
        {
            newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
            if (newbuf == 0)
                return (-1);
            strcpy(newbuf, *buf + i + 1);
            *msg = *buf;
            (*msg)[i + 1] = 0;
            *buf = newbuf;
            return (1);
        }
        i++;
    }
    return (0);
}

char *str_join(char *buf, char *add)
{
    char *newbuf;
    int len;

    if (buf == 0)
        len = 0;
    else
        len = strlen(buf);
    newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
    if (newbuf == 0)
        return (0);
    newbuf[0] = 0;
    if (buf != 0)
        strcat(newbuf, buf);
    free(buf);
    strcat(newbuf, add);
    return (newbuf);
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

int main(int argc, char **argv)
{
    int connfd;
    socklen_t len;
    struct sockaddr_in servaddr, cli;
    
    if (argc != 2)
    {
        write(2, "Wrong number of arguments\n", 26);
        exit(1);
    }
    
    for (int i = 0; i < 65536; i++)
    {
        clients[i].id = -1;
        clients[i].buffer = NULL;
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        fatal_error();
    
    max_fd = sockfd;
    FD_ZERO(&active_fds);
    FD_SET(sockfd, &active_fds);
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(2130706433);
    servaddr.sin_port = htons(atoi(argv[1]));
    
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
        fatal_error();
    
    if (listen(sockfd, 128) != 0)
        fatal_error();
    
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
```

## Desafíos Técnicos Clave

1. **🔄 Non-blocking I/O**: Usar `select()` para multiplexar
2. **💾 Gestión de memoria**: Evitar leaks en buffers de clientes  
3. **📝 Mensajes parciales**: Acumular datos hasta líneas completas
4. **📡 Broadcast eficiente**: Enviar a todos excepto remitente
5. **🔌 Desconexiones**: Detectar y limpiar recursos

## Compilación y Pruebas

```bash
# Compilar
gcc -Wall -Wextra -Werror mini_serv.c -o mini_serv

# Ejecutar servidor
./mini_serv 8081

# Probar con netcat (terminales separadas)
nc 127.0.0.1 8081
```

## Casos de Prueba Importantes

✅ **Conexión única**: Un cliente envía mensajes  
✅ **Múltiples clientes**: Intercambio de mensajes  
✅ **Desconexiones**: Clientes se van y vuelven  
✅ **Mensajes largos**: Requieren múltiples `recv()`  
✅ **Líneas múltiples**: Mensajes con `'\n'` interno  

## Consideraciones del Subject

- ❌ **Sin #define**: Usar valores literales
- 🏠 **Solo 127.0.0.1**: No otras interfaces  
- ⏰ **No desconectar lentos**: Aunque no lean mensajes
- ⚡ **Envío rápido**: Sin buffers innecesarios
- 💥 **Gestión errores**: `"Fatal error\n"` para syscalls

## Flujo del Programa

```
1. Validar argumentos (puerto)
2. Inicializar array de clientes (65536 entradas)
3. Crear y configurar socket servidor  
4. Inicializar fd_set active_fds
5. Loop infinito:
   a. Copiar active_fds a read_fds
   b. select() en read_fds
   c. Iterar sobre todos los fd's:
      - Si fd == sockfd → accept() y add_client()
      - Si fd es cliente → handle_client_data()
```

## Arquitectura Optimizada

La implementación actual usa un **diseño más eficiente**:

- **Array indexado por fd**: Acceso O(1) en lugar de búsqueda
- **fd_set global (`active_fds`)**: Mantiene estado persistente
- **Funciones modulares**: `add_client()`, `remove_client()`, `handle_client_data()`
- **Buffer grande (65536)**: Maneja mensajes extensos

Esta implementación crea un **servidor de chat funcional** que cumple todos los requisitos del ejercicio y puede manejar múltiples clientes concurrentemente.
