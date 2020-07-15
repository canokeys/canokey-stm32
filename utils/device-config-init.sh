#!/bin/bash
set -e
if [[ -z "$1" ]]; then
    exit 1
fi
echo "Reader name: $1"

if [[ "$GEN_CERT" != 1 ]]; then
    DEMO_ATTEST_KEY="00 01 00 00 20 cc d3 ee 4d ea 2a 3c ca d8 ce 3d 6a 47 f5 45 9c 80 79 7a d9 15 d6 f4 62 8b 5d 36 ef f2 76 d6 87"
    DEMO_ATTEST_CERT="00 02 00 00 00 02 7b 30 82 02 77 30 82 01 5f a0 03 02 01 02 02 01 0d 30 0d 06 09 2a 86 48 86 f7 0d 01 01 0b 05 00 30 31 31 2f 30 2d 06 03 55 04 03 0c 26 43 61 6e 6f 4b 65 79 73 20 46 49 44 4f 20 41 74 74 65 73 74 61 74 69 6f 6e 20 52 6f 6f 74 20 43 41 20 4e 6f 2e 31 30 1e 17 0d 32 30 30 37 31 35 30 31 35 35 30 30 5a 17 0d 33 30 30 34 31 34 30 31 35 35 30 30 5a 30 66 31 20 30 1e 06 03 55 04 03 0c 17 43 61 6e 6f 4b 65 79 20 53 65 72 69 61 6c 20 30 30 31 31 34 35 31 34 31 22 30 20 06 03 55 04 0b 0c 19 41 75 74 68 65 6e 74 69 63 61 74 6f 72 20 41 74 74 65 73 74 61 74 69 6f 6e 31 11 30 0f 06 03 55 04 0a 0c 08 43 61 6e 6f 4b 65 79 73 31 0b 30 09 06 03 55 04 06 13 02 43 4e 30 59 30 13 06 07 2a 86 48 ce 3d 02 01 06 08 2a 86 48 ce 3d 03 01 07 03 42 00 04 06 5c 77 8f 90 f3 5b 30 fc 64 c0 ff db 6a ea 64 bb c7 bd c5 63 89 01 60 96 c2 6d ac 83 cf 54 63 47 07 d0 57 72 e2 55 06 4a 55 c4 00 c7 d3 67 32 4a b6 26 82 e3 58 22 06 1e b9 9a 52 2c 97 54 99 a3 30 30 2e 30 09 06 03 55 1d 13 04 02 30 00 30 21 06 0b 2b 06 01 04 01 82 e5 1c 01 01 04 04 12 00 00 24 4e b2 9e e0 90 4e 49 81 fe 1f 20 f8 d3 b8 f4 30 0d 06 09 2a 86 48 86 f7 0d 01 01 0b 05 00 03 82 01 01 00 40 e1 20 47 e0 53 70 85 8c 1b db 55 db a6 8b 1e 4c a3 9a c6 e4 54 b5 d9 e9 35 65 04 7a c8 0a 3e 9a 9f 61 79 ec 86 d4 e5 87 20 a3 4b 1c 60 21 98 71 a4 6d c4 a4 5a 22 bd f4 aa c4 0a c4 b1 c3 5d ad 4c 1f 52 a0 ec 22 0c 53 38 54 57 55 2b 83 a6 71 9a ad 1d 03 1e a6 30 87 f7 17 d1 53 86 96 88 17 6d 14 4e 9e d5 b9 f2 50 38 5a 86 c6 75 50 fa 42 f9 1d ec 3d 03 35 13 d4 fc 20 fc 44 e4 86 cd a2 21 99 a6 1b 42 23 fe 56 36 6b 2c ed 45 39 fc 47 32 bb 25 92 08 fb 0f e6 c3 2f 14 3c 87 af f5 11 36 3a fc 5a 62 19 dd b3 b6 e4 b7 88 e3 7f 31 b6 a3 8a 24 79 10 1b 16 e0 ec 87 23 0c 48 b4 33 2a 9b 8c 78 fd 1e 91 fe 45 e6 eb 32 22 eb 91 72 0d e5 f2 1f 52 52 bf e7 5a 61 7b f7 15 c4 4b 01 48 8b 40 35 4e 39 8c 80 5c a7 99 df c6 4c 27 75 43 cd 1f 96 8d a1 f2 2c 9e a5 d1 ea 87 41 64 02"
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
