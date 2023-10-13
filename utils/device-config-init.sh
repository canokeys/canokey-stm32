#!/bin/bash
set -e
if [[ -z "$1" ]]; then
    exit 1
fi
echo "Reader name: $1"

if [[ "$GEN_CERT" != 1 ]]; then
    DEMO_ATTEST_KEY="00 01 00 00 20 46 5b 44 5d 8e 78 34 53 f7 4b 90 00 d2 20 32 51 99 5e 12 dc d1 21 a1 9c ea 09 5a fc f8 e9 eb 75"
    DEMO_ATTEST_CERT="00 02 00 00 00 02 7b 30 82 02 77 30 82 01 5f a0 03 02 01 02 02 01 15 30 0d 06 09 2a 86 48 86 f7 0d 01 01 0b 05 00 30 31 31 2f 30 2d 06 03 55 04 03 0c 26 43 61 6e 6f 4b 65 79 73 20 46 49 44 4f 20 41 74 74 65 73 74 61 74 69 6f 6e 20 52 6f 6f 74 20 43 41 20 4e 6f 2e 31 30 1e 17 0d 32 31 31 30 32 37 31 36 33 31 31 39 5a 17 0d 33 31 30 37 32 37 31 36 33 31 31 39 5a 30 66 31 20 30 1e 06 03 55 04 03 0c 17 43 61 6e 6f 4b 65 79 20 53 65 72 69 61 6c 20 31 31 31 31 31 31 31 31 31 22 30 20 06 03 55 04 0b 0c 19 41 75 74 68 65 6e 74 69 63 61 74 6f 72 20 41 74 74 65 73 74 61 74 69 6f 6e 31 11 30 0f 06 03 55 04 0a 0c 08 43 61 6e 6f 4b 65 79 73 31 0b 30 09 06 03 55 04 06 13 02 43 4e 30 59 30 13 06 07 2a 86 48 ce 3d 02 01 06 08 2a 86 48 ce 3d 03 01 07 03 42 00 04 2c 11 4a 50 45 41 4a 6b 22 8c 0c c4 f7 7a 18 fc 5d 1c 6e 97 54 e7 af 94 72 44 fe c7 60 7c ed 5a c8 a0 3a 74 e3 80 86 b1 b5 f2 d7 2e 5d 2a cf 51 77 38 2e 2f 60 76 e5 25 e7 9a 92 a5 a1 a6 0b 2c a3 30 30 2e 30 09 06 03 55 1d 13 04 02 30 00 30 21 06 0b 2b 06 01 04 01 82 e5 1c 01 01 04 04 12 04 10 24 4e b2 9e e0 90 4e 49 81 fe 1f 20 f8 d3 b8 f4 30 0d 06 09 2a 86 48 86 f7 0d 01 01 0b 05 00 03 82 01 01 00 7c 28 27 67 06 65 d6 31 78 da e8 9e c0 ac 93 c1 b5 d2 56 af f6 1d 0b 01 5d c1 1a 04 f5 c2 f7 00 9b ac f6 af e0 c9 23 93 b4 9f e0 7e b1 22 d8 be 0f a8 9d 32 5f 53 78 e4 11 90 b2 58 a1 0c 0f 0d 07 68 db ea 4e b5 0c ff 7e 7a 93 80 cc 51 a0 6d 49 1b 28 34 57 b5 cd f0 c3 1c 32 9e cb 5a 9d 32 44 d0 6b 9a 7b 7b 56 fe 6f ac b7 a8 51 55 57 39 5d e2 0e ba dd e8 25 64 72 9f 88 fc 93 1b ff 62 3e f2 d3 1c 6f d4 f7 be fc ea 51 86 bd ff 78 da ef 92 3b c2 3e e5 5c b9 3c 4c f2 ff 09 f8 77 a7 2d 87 7e 8d 3a 08 a2 ec 1d f4 5c b9 7c 8b 86 da c0 fb 5b b9 22 80 19 18 31 e4 69 a2 92 a4 7e 83 75 d0 60 5d 7d 41 9c bf 56 74 cb 6a c4 48 96 8c 8a 63 df e2 1c af 49 b9 3d af 29 86 0a 7a c7 8c 4e 73 05 a9 8d 1d b4 d4 33 6c 0b 64 af 3a b1 e1 b7 29 de 3b e6 6b db f2 59 a4 60 4c 68 72 47 7a"
fi
# env variable referenced in attestation-device-cert.cnf
export CANOKEY_SERIAL=$(printf %08x $(date +%s))

(cat <<-EOF
00A4040005F000000000
0020000006313233343536
00FF01010903B005720300B39900
00FF010103039100
00FF01010603A044000420
0030000004$CANOKEY_SERIAL
$DEMO_ATTEST_KEY
$DEMO_ATTEST_CERT
EOF
) | scriptor -r "$1"

if [[ -z "$DEMO_ATTEST_KEY" ]]; then
    ./gen_attestation_devive_cert.sh "$1"
fi
