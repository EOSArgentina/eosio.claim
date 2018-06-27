#include "eosio.unregd.hpp"

using eosio::unregd;

EOSIO_ABI(eosio::unregd, (add)(regaccount))

/**
 * Add a mapping between an ethereum_address and an initial EOS token balance.
 */
void unregd::add(const ethereum_address& ethereum_address, const asset& balance) {
  require_auth(_self);

  auto symbol = balance.symbol;
  eosio_assert(symbol.is_valid() && symbol == CORE_SYMBOL, "balance must be EOS token");

  eosio_assert(ethereum_address.length() == 42, "Ethereum address should have exactly 42 characters");

  update_address(ethereum_address, [&](auto& address) {
    address.ethereum_address = ethereum_address;
    address.balance = balance;
  });
}

/**
 * Register an EOS account using the stored information (address/balance) verifying an ETH signature
 */
void unregd::regaccount(const string& message, const bytes& pubkey, const bytes& signature, const string& account) {
   
  eosio_assert(message.size(), "Empty message");
  eosio_assert(pubkey.size() == 65, "Invalid pubkey");
  eosio_assert(signature.size() == 65, "Invalid signature");
  eosio_assert(account.size() == 12, "Invalid account length");

  // Verify that the destination account name is valid
  for(const auto& c : account) {
    if(!((c >= 'a' && c <= 'z') || (c >= '1' && c <= '5')))
      eosio_assert(false, "Invalid account name");
  }

  auto naccount = string_to_name(account.c_str());

  // Verify that the account does not exists
  eosio_assert(!is_account(naccount), "Account already exists");

  // Extract block prefix/num from message
  auto pos = message.find(",");
  eosio_assert( pos != string::npos,  "Malformed message" );

  auto msg_block_num = atoi(message.substr(0,pos).c_str());
  auto msg_block_prefix = atoi(message.substr(pos+1).c_str());

  // Verify that the current TX has the same block prefix/num as message
  eosio_assert( tapos_block_num() == msg_block_num, "Invalid block_num" );
  eosio_assert( tapos_block_prefix() == msg_block_prefix, "Invalid block_prefix" );

  // Calculate ETH address based on publickey
  sha3_ctx* shactx = (sha3_ctx*)malloc(sizeof(sha3_ctx));

  checksum256 msghash;
  rhash_keccak_256_init(shactx);
  rhash_keccak_update(shactx, (uint8_t*)&pubkey[1], 64);
  rhash_keccak_final(shactx, msghash.hash);

  uint8_t* eth_address = (uint8_t*)malloc(20);
  memcpy(eth_address, msghash.hash + 12, 20);

  // Verify that the ETH address exists in eosio.unregd contract
  addresses_index addresses(_self, _self);
  auto index = addresses.template get_index<N(ethereum_address)>();

  auto itr = index.find(compute_ethereum_address_key256(eth_address));
  eosio_assert(itr != index.end(), "Address not found");

  // Calculate hash(message) and verify ETH signature against publickey
  rhash_keccak_256_init(shactx);
  rhash_keccak_update(shactx, (uint8_t*)message.data(), message.size());
  rhash_keccak_final(shactx, msghash.hash);

  auto res = uECC_verify((const uint8_t *)&pubkey[1],
    msghash.hash, 32,
    (const uint8_t *)&signature[0],
    uECC_secp256k1()
  );

  eosio_assert(res == 1, "Signature verification failed");

  // Split contribution balance into cpu/net/liquid
  auto balances = split_snapshot(itr->balance);
  eosio_assert(balances.size() == 3, "Unable to split snapshot");
  eosio_assert(itr->balance == balances[0] + balances[1] + balances[2], "internal error");

  // Generate EOS key based on signature's pubkey
  std::array<char,33> eospub;
  uECC_compress((const uint8_t *)&pubkey[1], (uint8_t*)eospub.data(), uECC_secp256k1());

  // Calculate the amount of EOS to purchase 8k of RAM
  auto amount_to_purchase_8kb_of_RAM = buyrambytes(8*1024);

  // Build authority with just one k1 key
  auto auth = authority{1,{{{0,eospub},1}},{},{}};

  // Issue to eosio.unregd the necesary EOS to buy 8K of RAM
  INLINE_ACTION_SENDER(call::token, issue)( N(eosio.token), {{N(eosio),N(active)}},
    {N(eosio.unregd), amount_to_purchase_8kb_of_RAM,""});

  // Create account with the same key for owner/active
  INLINE_ACTION_SENDER(call::eosio, newaccount)( N(eosio), {{N(eosio.unregd),N(active)}},
    {N(eosio.unregd), naccount, auth, auth});

  // Buy RAM for this account (8k)
  INLINE_ACTION_SENDER(call::eosio, buyram)( N(eosio), {{N(eosio.unregd),N(active)}},
    {N(eosio.unregd), naccount, amount_to_purchase_8kb_of_RAM});

  // Delegate bandwith
  INLINE_ACTION_SENDER(call::eosio, delegatebw)( N(eosio), {{N(eosio.unregd),N(active)}},
    {N(eosio.unregd), naccount, balances[0], balances[1], 1});

  // Transfer remaining if any (liquid EOS)
  if( balances[2] != asset(0) ) {
    INLINE_ACTION_SENDER(call::token, transfer)( N(eosio.token), {{N(eosio.unregd),N(active)}},
    {N(eosio.unregd), naccount, balances[2], ""});
  }

  // Remove information for the ETH address from the eosio.unregd DB
  index.erase(itr);
}

void unregd::update_address(const ethereum_address& ethereum_address, const function<void(address&)> updater) {
  auto index = addresses.template get_index<N(ethereum_address)>();

  auto itr = index.find(compute_ethereum_address_key256(ethereum_address));
  if (itr == index.end()) {
    addresses.emplace(_self, [&](auto& address) {
      address.id = addresses.available_primary_key();
      updater(address);
    });
  } else {
    index.modify(itr, _self, [&](auto& address) { updater(address); });
  }
}
