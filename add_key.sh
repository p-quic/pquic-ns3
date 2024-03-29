#!/usr/bin/env bash

# Settings
knownhosts=$HOME/.ssh/known_hosts

if [ "x$1" == "x-h" ] || [ "x$1" == "x--help" ] || [ ${#1} == 0 ]; then
    echo "Usage: $0 <host> <fingerprint> [<port>]"
    echo "Example: $0 github.com nThbg6kXUpJWGl7E1IGOCspRomTxdCARLviKw6E5SY8"
    echo "The default port is 22."
    echo "The script will download the ssh keys from <host>, check if any match"
    echo "the <fingerprint>, and add that one to $knownhosts."
    exit 1
fi

# Argument handling
host=$1
fingerprint=$2
port=$(if [ -n "$3" ]; then echo "$3"; else echo 22; fi)

# Download the actual key (you cannot convert a fingerprint to the original key)
keys=$(ssh-keyscan -4 -p $port $host 2> /dev/null);
if [ ${#keys} -lt 20 ]; then echo Error downloading keys; exit 2; fi

# Find which line contains the key matching this fingerprint
line=$(ssh-keygen -lf <(echo "$keys") | grep -n "$fingerprint" | cut -b 1-1)

if [ ${#line} -gt 0 ]; then  # If there was a matching fingerprint
    # Take that line
    key=$(head -$line <(echo "$keys") | tail -1)
    # Check if the key part (column 3) of that line is already in $knownhosts
    if [ -n "$(grep "$(echo "$key" | awk '{print $3}')" $knownhosts)" ]; then
        echo "Key already in $knownhosts."
        exit 3
    else
        # Add it to known hosts
        echo "$key" >> $knownhosts
        # And tell the user what kind of key they just added
        keytype=$(echo "$key" | awk '{print $2}')
        echo Fingerprint verified and $keytype key added to $knownhosts
    fi
else  # If there was no matching fingerprint
    echo MITM? These are the received fingerprints:
    ssh-keygen -lf <(echo "$keys")
    echo Generated from these received keys:
    echo "$keys"
    exit 1
fi
