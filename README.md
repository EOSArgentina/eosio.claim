![Alt Text](https://i.imgur.com/6F5aHWH.png)

# Build

eosiocpp -o eosio.claim.wast eosio.claim.cpp

# Setup

```./setup.sh```

The setup step will install two contracts (besides the defaults ones):
  
  `eosio.unregd` (empty)
  `eosio.claim`

You need to have nodeos running.

# Test

```./test.sh```

The test step will:

 0. Generate a new ETH address with a random privkey.
 
 1. Add test data to the `eosio.unregd` DB using the ETH address.
    This is to simulate a user that contributed to the ERC20 but 
    didn't register their EOS pubkey.

 2. Call the `eosio.claim` with:

	* A message ("$block_num,$block_prefix" of the current LIB)
	* A signature for that message generated with the ETH privkey
	* The pubkey (uncompressed) derived from the ETH privkey
	* A desired EOS account name (random in this case)

4.  The contract will :
    * Verify that the account name is valid / and it has exactly 12 chars 
	* Verify that the desired account does not exists
	* Extract block num/prefix from message 
	* Assert that the current TX has the same block num/prefix as the message 
	* Calculate the ETH address based on the pubkey sent 
	* Verify that the ETH address has not claim their tokens before 
	* Find the ETH address in the eosio.unregd contract 
	* Calculate hash(message) and verify the signature against the pubkey provided 
	* Split eosio.unregd contribution into cpu/net/liquid (same split function as boot) 
	* Generate EOS key based on ETH pubkey 
	* Calculate the amount of EOS to purchase 8k of RAM (using the EOSRAM market) 
	* Build an authority with just one k1 key 
	* Issue (to eosio) the necesary amount of EOS to create the provided account (total = EOS to buy 8k of RAM + balance of account in eosio.unregd) 
	* Create an EOS account with the above authority for owner/active 
	* Buy RAM for the provided account (8k) 
	* Delegate bandwith 
	* Transfer remaining EOS if any (liquid EOS) 
	* Mark the ETH address as "used"
