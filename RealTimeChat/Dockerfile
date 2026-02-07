FROM gcc:latest

RUN apt-get update && apt-get install -y \
    libasio-dev \
    libssl-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/app

COPY . .

# 1. ESKI FAYLNI O'CHIRAMIZ (Juda muhim!)
RUN rm -f httplib.h

# 2. YANGISINI YUKLAYMIZ
RUN wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -O httplib.h

# 3. KOMPILYATSIYA
RUN g++ -o chat_server RealTimeChat.cpp -lpthread -latomic -lssl -lcrypto -std=c++17

EXPOSE 8080

CMD ["./chat_server"]