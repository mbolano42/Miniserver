/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   mini_serv.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mbolano- <mbolano-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/27 13:33:34 by mbolano-          #+#    #+#             */
/*   Updated: 2025/11/27 13:33:37 by mbolano-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
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
