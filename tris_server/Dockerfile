FROM gcc:latest

# Installo le dipendenze necessarie incluso netcat per healthcheck
RUN apt-get update && apt-get install -y \
    uuid-dev \
    netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copio tutti i sorgenti C dalla directory corrente
COPY *.c *.h ./

# Compilo il server
RUN gcc -Wall -Wextra -O2 -o tris_server *.c -lpthread

# La porta su cui ascolta il server
EXPOSE 12345

# Comando di avvio
CMD ["./tris_server"]
