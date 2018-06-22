#!/bin/sh
PRIV=$(python -c 'import sys; import bitcoin as b; print b.encode_privkey(sys.argv[1],"wif")' $(openssl rand -hex 32))

ACCOUNT=$(openssl rand -hex 32 | base32 | tr '[:upper:]' '[:lower:]' | head -c12)
ADDY=$(python get_eth_address.py $PRIV)
AMOUNT=$(python -c 'import random; print "%.4f" % (random.random()*1000)')

# Add test unregd data for this address
echo "* Adding $ACCOUNT to eosio.unregd db with $AMOUNT EOS"
cleos push action eosio.unregd add '["'$ADDY'","'$AMOUNT' EOS"]' -p eosio.unregd 2>&1 > /dev/null

# Claim
r=-1
while [ "$r" != "0" ]; do
  sleep 0.5
  python claim.py $PRIV $ACCOUNT 2> /dev/null
  r=$?
done
cleos get account $ACCOUNT
echo "* $ACCOUNT registered with $AMOUNT EOS"

# Transfer to test ETH privkey
cleos wallet import $PRIV
cleos transfer $ACCOUNT thisisatesta "0.0001 EOS"