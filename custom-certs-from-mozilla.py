#!/usr/bin/env python3

# This script pulls the list of Mozilla trusted certificate authorities
# from the web at the "mozurl" below, parses the file to grab the PEM
# for each cert, and then generates DER files in a new ./data directory
# Upload these to an on-chip filesystem and use the CertManager to parse
# and use them for your outgoing SSL connections.
#
# Script by Earle F. Philhower, III.  Released to the public domain.
#
# Updated by Federico Martinez to only add certain certificates

from __future__ import print_function
import csv
import os
import sys
from shutil import which

from subprocess import Popen, PIPE, call
try:
    from urllib.request import urlopen
except Exception:
    from urllib2 import urlopen
try:
    from StringIO import StringIO
except Exception:
    from io import StringIO

# check if ar and openssl are available
if which('ar') is None and not os.path.isfile('./ar') and not os.path.isfile('./ar.exe'):
    raise Exception("You need the program 'ar' from xtensa-lx106-elf found here: (esp8266-arduino-core)/hardware/esp8266com/esp8266/tools/xtensa-lx106-elf/xtensa-lx106-elf/bin/ar")
if which('openssl') is None and not os.path.isfile('./openssl') and not os.path.isfile('./openssl.exe'):
    raise Exception("You need to have openssl in PATH, installable from https://www.openssl.org/")

# Mozilla's URL for the CSV file with included PEM certs
mozurl = "https://ccadb-public.secure.force.com/mozilla/IncludedCACertificateReportPEMCSV"

# Load the names[] and pems[] array from the URL
names = []
pems = []
response = urlopen(mozurl)
csvData = response.read()
if sys.version_info[0] > 2:
    csvData = csvData.decode('utf-8')
csvFile = StringIO(csvData)
csvReader = csv.reader(csvFile)
rowCount = 0
CAsToAdd = ["IdenTrust Services, LLC", "Internet Security Research Group", "Baltimore CyberTrust Root", "Baltimore"] # row[0] is "Owner" and row[1] is "Certificate Issuer Organization" (Baltimore is only found in row[1])
for row in csvReader:
    if rowCount != 0:
        if row[0] in CAsToAdd or row[1] == "Baltimore":
            print(f"Adding {row[0]}")
            names.append(row[0]+":"+row[1]+":"+row[2])
            for item in row:
                if item.startswith("'-----BEGIN CERTIFICATE-----"):
                    pems.append(item)
    rowCount += 1
# del names[0] # Remove headers
# del pems[0] # Remove headers

# Try and make ./data, skip if present
try:
    os.mkdir("data")
except Exception:
    pass

derFiles = []
idx = 0
# Process the text PEM using openssl into DER files

"""
print("Items in pems: ")
for i in pems:
    print(i)

print("Items in names: ")
for i in names:
    print(i)
"""

pemsLen = len(pems)
print(f"pemsLen: {pemsLen}")
for i in range(0, pemsLen):
    certName = "data/ca_%03d.der" % (idx);
    thisPem = pems[i].replace("'", "")
    print(names[i] + " -> " + certName)
    ssl = Popen(['openssl','x509','-inform','PEM','-outform','DER','-out', certName], shell = False, stdin = PIPE)
    pipe = ssl.stdin
    pipe.write(thisPem.encode('utf-8'))
    pipe.close()
    ssl.wait()
    if os.path.exists(certName):
        derFiles.append(certName)
        idx = idx + 1

if os.path.exists("data/certs.ar"):
    print("certs.ar already exists. Replacing it..")
    os.unlink("data/certs.ar");

arCmd = ['ar', 'q', 'data/certs.ar'] + derFiles;
call( arCmd )

for der in derFiles:
    os.unlink(der)

print("Done")
