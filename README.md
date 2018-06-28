![Alt Text](https://i.imgur.com/6F5aHWH.png)

# Build

eosiocpp -o eosio.unregd.wast eosio.unregd.cpp

# Setup

```./setup.sh```

The setup step will install one contract (besides the defaults ones):
  
  `eosio.unregd` (empty)

You need to have nodeos running.

# Dependecies

 ```pip install bitcoin --user```
 ```pip install requests --user```
 ```sudo apt-get install python-pysha3```

# Test

```./test.sh```

The test step will:

 0. Generate a new ETH address with a random privkey.
 
 1. Add the ETH address (`eosio.unregd::add`) and transfers a random amount of 
    EOS to the `eosio.unregd` contract.
    This is to simulate a user that contributed to the ERC20 but 
    didn't register their EOS pubkey.

 2. Call `eosio.unregd::regaccount` with:

	* A message ("$block_num,$block_prefix" of the current LIB)
	* A signature for that message generated with the ETH privkey
	* The pubkey (uncompressed) derived from the ETH privkey
	* A desired EOS account name (random in this case)

4.  The function will :
  
  * Verify that the destination account name is valid
  * Verify that the account does not exists
  * Extract block prefix/num from message
  * Verify that the current TX has the same block prefix/num as message
  * Calculate ETH address based on publickey
  * Verify that the ETH address exists in eosio.unregd contract
  * Calculate hash(message) and verify ETH signature against publickey
  * Split contribution balance into cpu/net/liquid
  * Generate EOS key based on signature's pubkey
  * Calculate the amount of EOS to purchase 8k of RAM
  * Build authority with just one k1 key
  * Issue to eosio.unregd the necesary EOS to buy 8K of RAM
  * Create account with the same key for owner/active
  * Buy RAM for the account (8k)
  * Delegate bandwith
  * Transfer remaining if any (liquid EOS)
  * Remove information for the ETH address from the eosio.unregd DB
