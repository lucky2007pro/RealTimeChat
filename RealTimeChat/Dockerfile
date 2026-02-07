FROM gcc:latest

RUN apt-get update && apt-get install -y \
    libasio-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/app

COPY . .

RUN g++ -o chat_server main.cpp -lpthread -latomic -std=c++17

EXPOSE 8080

CMD ["./chat_server"]