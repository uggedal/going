#!/usr/bin/env bash

echo "hello from $1"
sleep $[ ( $RANDOM % 10 )  + 1 ]s
