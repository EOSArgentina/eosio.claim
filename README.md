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

 0) Generate a new ETH address with a random privkey.
 
 1) Add test data to the `eosio.unregd` DB using the ETH address.
    This is to simulate a user that contributed to the ERC20 but 
    didnt register their EOS pubkey.

 2) Call the `eosio.claim` with:

	a) A message ("$block_num,$block_prefix" of the current LIB)
	b) A signature for that message generated with the ETH privkey
	c) The pubkey (uncompressed) derived from the ETH privkey
	d) A desired EOS account name (random in this case)

4.  The contract will :
    
	a) Verify that the account name is valid / and it has exactly 12 chars 
	b) Verify that the desired account does not exists 
	c) Extract block num/prefix from message 
	d) Assert that the current TX has the same block num/prefix as the message 
	e) Calculate the ETH address based on the pubkey sent 
	f) Verify that the ETH address has not claim their tokens before 
	g) Find the ETH address in the eosio.unregd contract 
	h) Calculate hash(message) and verify the signature against the pubkey provided 
	i) Split eosio.unregd contribution into cpu/net/liquid (same split function as boot) 
	j) Generate EOS key based on ETH pubkey 
	k) Calculate the amount of EOS to purchase 8k of RAM (using the EOSRAM market) 
	l) Build an authority with just one k1 key 
	m) Issue (to eosio) the necesary amount of EOS to create the provided account (total = EOS to buy 8k of RAM + balance of account in eosio.unregd) 
	n) Create an EOS account with the above authority for owner/active 
	o) Buy RAM for the provided account (8k) 
	p) Delegate bandwith 
	q) Transfer remaining EOS if any (liquid EOS) 
	r) Mark the ETH address as "used"
