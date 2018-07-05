#include "eosio.unregd.hpp"
#include <eosiolib/crypto.h>
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
void unregd::regaccount(const bytes& signature, const string& account) {

  eosio_assert(signature.size() == 66, "Invalid signature");
  eosio_assert(account.size() == 12, "Invalid account length");

  // Verify that the destination account name is valid
  for(const auto& c : account) {
    if(!((c >= 'a' && c <= 'z') || (c >= '1' && c <= '5')))
      eosio_assert(false, "Invalid account name");
  }

  auto naccount = string_to_name(account.c_str());

  // Verify that the account does not exists
  eosio_assert(!is_account(naccount), "Account already exists");

  // Calculate message hash based on current TX block num/prefix
  char* message = (char*)malloc(64);
  sprintf(message, "%u,%u", tapos_block_num(), tapos_block_prefix());

  checksum256 digest;
  sha256(message, strlen(message), &digest);

  // Recover compressed pubkey from signature
  uint8_t* pubkey = (uint8_t*)malloc(64);
  uint8_t* compressed_pubkey = (uint8_t*)malloc(34);
  auto res = recover_key(
    &digest,
    signature.data(),
    signature.size(),
    (char*)compressed_pubkey,
    34
  );

  eosio_assert(res == 34, "Recover key failed");

  // Decompress pubkey
  uECC_decompress(compressed_pubkey+1, pubkey, uECC_secp256k1());

  // Calculate ETH address based on decompressed pubkey
  sha3_ctx* shactx = (sha3_ctx*)malloc(sizeof(sha3_ctx));

  checksum256 msghash;
  rhash_keccak_256_init(shactx);
  rhash_keccak_update(shactx, pubkey, 64);
  rhash_keccak_final(shactx, msghash.hash);

  uint8_t* eth_address = (uint8_t*)malloc(20);
  memcpy(eth_address, msghash.hash + 12, 20);

  // Verify that the ETH address exists in eosio.unregd contract
  addresses_index addresses(_self, _self);
  auto index = addresses.template get_index<N(ethereum_address)>();

  auto itr = index.find(compute_ethereum_address_key256(eth_address));
  eosio_assert(itr != index.end(), "Address not found");

  // Split contribution balance into cpu/net/liquid
  auto balances = split_snapshot(itr->balance);
  eosio_assert(balances.size() == 3, "Unable to split snapshot");
  eosio_assert(itr->balance == balances[0] + balances[1] + balances[2], "internal error");

  // Calculate the amount of EOS to purchase 8k of RAM
  auto amount_to_purchase_8kb_of_RAM = buyrambytes(8*1024);

  // Build authority with just one k1 key
  auto eospub = reinterpret_cast<std::array<char,33>*>(compressed_pubkey+1);
  auto auth = authority{1,{{{0,*eospub},1}},{},{}};

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
