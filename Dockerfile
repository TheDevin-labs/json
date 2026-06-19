FROM opensuse/tumbleweed:latest

RUN zypper refresh && \
    zypper --non-interactive install lua54

WORKDIR /usr/src/json-master

 
COPY . .

CMD [ "lua5.4" ]
