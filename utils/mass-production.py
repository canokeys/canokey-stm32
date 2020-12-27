#!/usr/bin/env python3
import os
import re
import sys
import time
import binascii
import argparse
import traceback
import subprocess as sp
from smartcard.System import readers

RE_PRIKEY_HEX = re.compile(r"priv((:\s*\w\w){32})", re.MULTILINE)

def gen_ca():
    choice = input("CA key/certificate will be over-written, continue? [y/N] ").lower()
    if not choice.startswith('y'):
        return
    sp.run("openssl ecparam -name secp256r1 -genkey -out ca.key", shell=True, check=True)
    sp.run("openssl req -config attestation-ca-cert.cnf -extensions ca_extensions_sec -x509 -days 7120 -new -key ca.key -nodes -out ca.pem", shell=True, check=True)
    with open("ca.srl", "w") as srl:
        srl.write("01")
    print("Generated files: ca.key, ca.pem, ca.srl")

def gen_device_key(serial):
    os.environ["CANOKEY_SERIAL"] = serial
    sp.run("openssl ecparam -out ec_key.pem -name secp256r1 -genkey -out dev.key", shell=True, check=True)
    sp.run("openssl req -config attestation-device-cert.cnf -new -key dev.key -nodes -out dev.csr", shell=True, check=True)
    sp.run("openssl x509 -extfile attestation-device-cert.cnf -extensions extensions_sec -days 3560 -req -in dev.csr -CA ca.pem -CAserial ca.srl -CAkey ca.key -out dev.pem", shell=True, check=True)
    sp.run("openssl x509 -outform der -in dev.pem -out dev.der", shell=True, check=True)
    ret = sp.run("openssl ec -in dev.key -text", shell=True, check=True, capture_output=True)
    out = ret.stdout.decode('ascii')
    priv = RE_PRIKEY_HEX.search(out)
    if priv is None:
        raise ValueError("Failed to parse private key from openssl output")
    dev_key = re.sub(r'[\s:]', '', priv.group(1))
    assert len(dev_key) == 64

    with open("dev.der", "rb") as cert:
        dev_cert = binascii.hexlify(cert.read()).decode('ascii')
    return dev_key, dev_cert

def transmit_apdu(connection, apdu):
    data, sw1, sw2 = connection.transmit(list(binascii.unhexlify(apdu)))
    if (sw1, sw2) != (0x90, 0):
        raise ValueError("APDU {} failed with {:02x}{:02x}".format(apdu, sw1, sw2))

def write_configs(connection):
    transmit_apdu(connection, "00A4040005F000000000")
    transmit_apdu(connection, "0020000006313233343536")
    transmit_apdu(connection, "00FF01000603A044000420")
    transmit_apdu(connection, "00FF01000903B005720300B39900")

def mp_procedure(reader_name):
    print("Input the serial: ")
    serial_hex = input()
    print("\n------")
    print("Current serial: {}".format(serial_hex))
    input("Insert a canokey then press enter to continue, Ctrl-C to exit")

    try:
        r = readers()
        if len(r) == 0:
            print("No readers available")
            return False
        for reader in r:
            if reader_name in str(reader):
                print("Target:", reader)
                break
        else:
            print("No matching reader. Available readers:", r)
            return False
        dev_key, dev_cert = gen_device_key(serial_hex)
        connection = reader.createConnection()
        connection.connect()
        write_configs(connection)
        transmit_apdu(connection, "0030000004{}".format(serial_hex))
        transmit_apdu(connection, "0001000020{}".format(dev_key))
        transmit_apdu(connection, "0002000000{:04X}{}".format(len(dev_cert)//2, dev_cert))
        connection.disconnect()
        print("Successfully initialized. Next.")
    except:
        print("Failed to initialize the canokey")
        traceback.print_exc()
        return False

    return True

if __name__ == "__main__":
    os.chdir(os.path.dirname(__file__))
    parser = argparse.ArgumentParser(description='Initialize canokeys with atestation certificate')
    parser.add_argument('--gen-ca', action='store_true', help='generate atestation CA key/certificate')
    parser.add_argument('--reader', type=str, default='OpenPGP PIV OATH', help='CCID reader name of canokeys')
    args = parser.parse_args()
    if args.gen_ca:
        gen_ca()
    else:
        while True:
            succ = mp_procedure(args.reader)
            if not succ:
                time.sleep(0.5)
