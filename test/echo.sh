#!/usr/bin/env bash

echo "echo from child"
sleep $[ ( $RANDOM % 10 )  + 1 ]s
