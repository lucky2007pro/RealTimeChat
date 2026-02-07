FROM gcc:latest

# 1. wget va OpenSSL o'rnatamiz
RUN apt-get update && apt-get install -y \
    libasio-dev \
    libssl-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/app

COPY . .

# 2. ENG MUHIM JOYI: 
# httplib.h ni GitHubdan to'g'ridan-to'g'ri yuklab olib, eskisini ustiga yozamiz.
# Bu "set_header" xatosini 100% tuzatadi.
RUN wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -O httplib.h

# 3. Kompilyatsiya
RUN g++ -o chat_server RealTimeChat.cpp -lpthread -latomic -lssl -lcrypto -std=c++17

EXPOSE 8080

CMD ["./chat_server"]