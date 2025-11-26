#!/bin/bash

echo "=== MINI_SERV TEST SCRIPT ==="
echo "Este script compila y prueba el servidor mini_serv"
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check for running processes that could interfere
echo -e "${YELLOW}[0] Verificando procesos existentes...${NC}"
RUNNING_MINI_SERV=$(ps aux | grep -E '[m]ini_serv' | grep -v grep)
RUNNING_NC=$(ps aux | grep -E '\bnc\b.*127\.0\.0\.1|localhost' | grep -v grep)

if [ ! -z "$RUNNING_MINI_SERV" ]; then
    echo -e "${YELLOW}⚠ Se encontraron procesos mini_serv en ejecución:${NC}"
    echo "$RUNNING_MINI_SERV"
    echo -e "${YELLOW}¿Deseas eliminarlos? (s/n)${NC}"
    read -t 5 -n 1 response || response="s"
    echo
    if [ "$response" = "s" ] || [ "$response" = "S" ] || [ -z "$response" ]; then
        killall -9 mini_serv 2>/dev/null
        echo -e "${GREEN}✓ Procesos mini_serv eliminados${NC}"
    else
        echo -e "${RED}✗ Los procesos existentes pueden causar fallos en las pruebas${NC}"
    fi
fi

if [ ! -z "$RUNNING_NC" ]; then
    echo -e "${YELLOW}⚠ Se encontraron conexiones netcat activas${NC}"
    killall -9 nc 2>/dev/null
    echo -e "${GREEN}✓ Conexiones netcat eliminadas${NC}"
fi

sleep 0.5
echo

# Compile the server
echo -e "${YELLOW}[1] Compilando mini_serv...${NC}"
gcc -Wall -Wextra -Werror mini_serv.c -o mini_serv

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Compilación exitosa${NC}"
else
    echo -e "${RED}✗ Error de compilación${NC}"
    exit 1
fi

echo

# Test wrong arguments
echo -e "${YELLOW}[2] Probando argumentos incorrectos...${NC}"
./mini_serv 2>&1 | grep -q "Wrong number of arguments"
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Manejo correcto de argumentos incorrectos${NC}"
else
    echo -e "${RED}✗ No maneja correctamente argumentos incorrectos${NC}"
fi

./mini_serv 1 2 3 2>&1 | grep -q "Wrong number of arguments"
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Manejo correcto de demasiados argumentos${NC}"
else
    echo -e "${RED}✗ No maneja correctamente demasiados argumentos${NC}"
fi

echo

# Test server startup
echo -e "${YELLOW}[3] Probando inicio del servidor...${NC}"
timeout 2 ./mini_serv 8081 &
SERVER_PID=$!
sleep 1

if ps -p $SERVER_PID > /dev/null; then
    echo -e "${GREEN}✓ Servidor inicia correctamente en puerto 8081${NC}"
    kill $SERVER_PID 2>/dev/null
else
    echo -e "${RED}✗ El servidor no inicia correctamente${NC}"
fi

echo

# Test invalid port (should show Fatal error)
echo -e "${YELLOW}[4] Probando puerto inválido...${NC}"
timeout 2 ./mini_serv 99999 2>&1 | grep -q "Fatal error"
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Manejo correcto de puerto inválido${NC}"
else
    echo -e "${YELLOW}~ Puerto inválido no genera Fatal error (puede ser normal)${NC}"
fi

echo

# Pruebas funcionales automáticas con dos clientes
echo -e "${YELLOW}[5] Pruebas funcionales automáticas (2 clientes)...${NC}"
PORT=8082
# Start server in background
./mini_serv $PORT &
SERVER_PID=$!
sleep 0.5

if ! ps -p $SERVER_PID > /dev/null; then
    echo -e "${RED}✗ No se pudo iniciar el servidor para pruebas funcionales${NC}"
else
    TMPDIR=$(mktemp -d)
    c1_out=$TMPDIR/c1_out
    c2_out=$TMPDIR/c2_out
    
    # Use bash TCP sockets for more reliable testing
    # Client 1: connects, waits for client 2 arrival, sends message, disconnects
    {
        exec 3<>/dev/tcp/127.0.0.1/$PORT
        sleep 0.5  # Wait for client 2 to connect
        echo "hello from client 0" >&3
        sleep 0.3
        cat <&3 &
        CAT_PID=$!
        sleep 0.5
        kill $CAT_PID 2>/dev/null || true
        exec 3<&-
    } > $c1_out 2>/dev/null &
    C1_PID=$!
    
    sleep 0.2
    
    # Client 2: connects, receives messages from client 1 and left notification
    {
        exec 4<>/dev/tcp/127.0.0.1/$PORT
        cat <&4 &
        CAT_PID=$!
        sleep 1.5
        kill $CAT_PID 2>/dev/null || true
        exec 4<&-
    } > $c2_out 2>/dev/null &
    C2_PID=$!
    
    wait $C1_PID 2>/dev/null
    wait $C2_PID 2>/dev/null
    
    sleep 0.3

    # Check if client 1 received arrival notification of client 1 (second client)
    if grep -q "server: client 1 just arrived" $c1_out; then
        echo -e "${GREEN}✓ Broadcast de llegada recibido por cliente existente${NC}"
    else
        echo -e "${YELLOW}⚠ No se detectó broadcast de llegada${NC}"
        echo "Debug c1_out:"; cat $c1_out
    fi

    # Check if client 2 received message from client 1
    if grep -q "client 0: hello from client 0" $c2_out; then
        echo -e "${GREEN}✓ Mensaje retransmitido correctamente entre clientes${NC}"
    else
        echo -e "${YELLOW}⚠ No se detectó retransmisión de mensaje${NC}"
        echo "Debug c2_out:"; cat $c2_out
    fi
    
    # Check disconnection notification
    if grep -q "server: client 0 just left" $c2_out; then
        echo -e "${GREEN}✓ Notificación de desconexión recibida correctamente${NC}"
    else
        echo -e "${YELLOW}⚠ No se detectó notificación de desconexión${NC}"
    fi

    # Cleanup
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null
    rm -rf $TMPDIR
fi

echo

echo -e "${YELLOW}[6] Instrucciones para prueba manual:${NC}"
echo "Para probar manualmente el servidor:"
echo
echo "1. Ejecutar el servidor:"
echo "   ./mini_serv 8081"
echo
echo "2. En otra terminal, conectar con netcat:"
echo "   nc 127.0.0.1 8081"
echo
echo "3. Abrir más terminales y conectar más clientes"
echo
echo "4. Escribir mensajes en cualquier cliente y ver cómo se propagan"
echo
echo "Ejemplo de salida esperada:"
echo "- Cliente 1 se conecta: 'server: client 0 just arrived'"
echo "- Cliente 2 se conecta: 'server: client 1 just arrived'"
echo "- Cliente 1 envía 'hola': Cliente 2 recibe 'client 0: hola'"
echo "- Cliente 1 se desconecta: Cliente 2 recibe 'server: client 0 just left'"

echo
echo -e "${GREEN}=== TESTS COMPLETADOS ===${NC}"
echo "Revisa los resultados arriba y prueba manualmente el servidor."
