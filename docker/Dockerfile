FROM debian:stretch
LABEL maintainer="405Coin Developers <dev@405Coin.org>"
LABEL description="Dockerised 405CoinCore, built from Travis"

RUN apt-get update && apt-get -y upgrade && apt-get clean && rm -fr /var/cache/apt/*

COPY bin/* /usr/bin/
