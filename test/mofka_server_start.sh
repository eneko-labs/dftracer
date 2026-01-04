#!/bin/bash

# bedrock tcp -c mofka.config.json -v trace 1> mofka_server.log 2>&1 &

$1 tcp -c $2 -v trace 1> $3 2>&1 &
sleep 1
