FROM ubuntu

MAINTAINER Shamil Zubairov <zubairov.sh@gmail.com>

RUN apt-get update && apt-get install build-essential -y

RUN groupadd -r gshamil && useradd -r -g gshamil shamil

# Рабочий каталог для всех последующих инструкций RUN, CMD, ENTRYPOINT, ADD, COPY
WORKDIR /CLC

COPY / /CLC

EXPOSE 7654

# CMD "./start"