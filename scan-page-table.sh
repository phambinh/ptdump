#!/usr/bin/env bash

INTERVAL=5
./mcf inp.in &
APP_PID=$!
./scan-page-table $APP_PID $INTERVAL
