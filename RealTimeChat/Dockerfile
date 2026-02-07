FROM gcc:latest

# OpenSSL (libssl-dev) qo'shildi!
RUN apt-get update && apt-get install -y \
    libasio-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/app

COPY . .

# -lssl -lcrypto qo'shildi!
RUN g++ -o chat_server RealTimeChat.cpp -lpthread -latomic -lssl -lcrypto -std=c++17

EXPOSE 8080

CMD ["./chat_server"]