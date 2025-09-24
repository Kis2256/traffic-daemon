FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive


RUN apt-get update && \
    apt-get install -y build-essential gcc make cmake iproute2 iputils-ping && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app


COPY CMakeLists.txt /app/
COPY *.c /app/
COPY *.h /app/


RUN mkdir build
WORKDIR /app/build


RUN cmake .. && make

