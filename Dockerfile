FROM opensuse/tumbleweed:latest

RUN zypper refresh && \
    zypper --non-interactive install lua54

WORKDIR /app

COPY json/ .

ENV LUA_PATH="./?.lua;;"

CMD [ "lua5.4", "-e", "local j = require('json'); print('JSON Library Loaded Successfully inside Tumbleweed!')" ]
