FROM opensuse/tumbleweed:latest

RUN zypper refresh && \
    zypper --non-interactive install lua54 curl

WORKDIR /app

COPY . .

CMD [ "lua5.4" ]
