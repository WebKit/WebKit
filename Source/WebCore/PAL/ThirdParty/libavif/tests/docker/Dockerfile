FROM ubuntu:20.04

ADD build.sh /root/build.sh
RUN apt update && apt install -y dos2unix
RUN dos2unix /root/build.sh
RUN bash /root/build.sh
