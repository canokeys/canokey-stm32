#!/bin/bash
set -e
if [[ -z "$CANOKEY_SERIAL" ]]; then
    echo "Env CANOKEY_SERIAL should be set"
    exit 1
fi
openssl ecparam -out ec_key.pem -name secp256r1 -genkey -out dev.key
openssl req -config ./attestation-device-cert.cnf -new -key dev.key -nodes -out dev.csr
openssl x509 -extfile ./attestation-device-cert.cnf -extensions extensions_sec -days 3560 -req -in dev.csr -CA ca.pem -CAserial ca.srl -CAkey ca.key -out dev.pem
openssl x509 -outform der -in dev.pem -out dev.der
rm dev.csr

cert=$(xxd -p -c 1000000 dev.der)
size=$(wc dev.der |awk '{printf("%04x\n",$3)}')
priv=$(openssl ec -in dev.key -text | grep -A 3 'priv:'|tail -n 3|tr -d -C '[:alnum:]')

if [[ ${#priv} != 64 ]]; then
    echo "ECDSA key should be 32-bytes long"
    exit 1
fi

if [[ -n "$1" ]]; then

    (cat <<-EOF
00A4040005F000000000
0020000006313233343536
0001000020$priv
0002000000$size$cert
EOF
    ) | scriptor -r "$1"
fi