#!/bin/bash
if [ -z "$1" ]; then
    exit 1
fi
echo "Reader name: $1"
if [ "$2" == 0 ]; then
    cmd=00FF0355
else
    cmd=00FF036604
fi
(cat <<-EOF
00A4040005F000000000
0020000006313233343536
$cmd
EOF
) | scriptor -r "$1"

# Stack usage results:
# OpenPGP: 3560
# PIV: 3656
# FIDO2: 3688
# OATH: 1936
# NDEF: 1088
