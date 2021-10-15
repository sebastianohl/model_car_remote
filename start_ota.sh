#!/bin/sh

mosquitto_pub -h $1 -u $2 -P $3 -r -t homie/sentio-b2-control/sentio-b2-control/update/set -m $4 -q 0
