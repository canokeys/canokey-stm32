#!/bin/bash
set -e
openssl genrsa -out ca.key 2048
openssl req -config ./attestation-ca-cert.cnf -extensions ca_extensions_sec -x509 -days 7120 -new -key ca.key -nodes -out ca.pem
echo 01 >ca.srl # create the serial file