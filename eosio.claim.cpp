#include <functional>
#include <string>
#include <cmath>

#include <eosiolib/transaction.h>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/fixed_key.hpp>
#include <eosiolib/public_key.hpp>

#include "ram/exchange_state.cpp"

#define USE_KECCAK
#include "sha3/byte_order.c"
#include "sha3/sha3.c"

#define uECC_SUPPORTS_secp160r1 0
#define uECC_SUPPORTS_secp192r1 0
#define uECC_SUPPORTS_secp224r1 0
#define uECC_SUPPORTS_secp256r1 0
#define uECC_SUPPORTS_secp256k1 1
#define uECC_SUPPORT_COMPRESSED_POINT 1
#include "ecc/uECC.c"

using namespace eosio;
using namespace std;

#include "utils/unregd.hpp"
#include "utils/inline_calls_helper.hpp"
#include "utils/snapshot.hpp"

namespace eosio {

class claim : public eosio::contract {
  public:
      using contract::contract;

      //@abi table used i64
      struct used {
         uint64_t id;
         bytes ethereum_address;

         uint64_t primary_key() const { return id; }
         key256 by_ethereum_address() const { return compute_ethereum_address_key256((uint8_t*)ethereum_address.data()); }

         EOSLIB_SERIALIZE(used, (id)(ethereum_address))
      };

      typedef eosio::multi_index<
         N(used), used,
         indexed_by<N(ethereum_address), const_mem_fun<used, key256, &used::by_ethereum_address>>
      > used_index;

      void regaccount(const string& message, const bytes& pubkey, const bytes& signature, const string& account) {
         
         eosio_assert(message.size(), "Empty message");
         eosio_assert(pubkey.size() == 65, "Invalid pubkey");
         eosio_assert(signature.size() == 65, "Invalid signature");
         eosio_assert(account.size() == 12, "Invalid account length");

         // Verify that the account name is valid 
         // and it has exactly 12 chars
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

         // eosio::print(
         //    "\nmsg_block_num:", (uint64_t)msg_block_num,
         //    "\nmsg_block_prefix:", (uint64_t)msg_block_prefix,
         //    "\ntapos_block_num:", (uint64_t)tapos_block_num(),
         //    "\ntapos_block_prefix:", (uint64_t)tapos_block_prefix()
         // );

         // Assert that the user signed block num/prefix
         eosio_assert( tapos_block_num() == msg_block_num, "Invalid block_num" );
         eosio_assert( tapos_block_prefix() == msg_block_prefix, "Invalid block_prefix" );

         // Calculate ETH address
         sha3_ctx* shactx = (sha3_ctx*)malloc(sizeof(sha3_ctx));

         checksum256 msghash;
         rhash_keccak_256_init(shactx);
         rhash_keccak_update(shactx, (uint8_t*)&pubkey[1], 64);
         rhash_keccak_final(shactx, msghash.hash);

         uint8_t* eth_address = (uint8_t*)malloc(20);
         memcpy(eth_address, msghash.hash + 12, 20);

         // Verify that the ETH address hasnt claim their tokens before
         used_index used_address(_self, _self);
         auto uindex = used_address.template get_index<N(ethereum_address)>();
         auto uitr = uindex.find(compute_ethereum_address_key256(eth_address));
         eosio_assert(uitr == uindex.end(), "Address already used");

         // Find ETH address in eosio.unregd contract
         addresses_index addresses(N(eosio.unregd), N(eosio.unregd));
         auto index = addresses.template get_index<N(ethereum_address)>();

         auto itr = index.find(compute_ethereum_address_key256(eth_address));
         eosio_assert(itr != index.end(), "Address not found");

         // Calculate hash(message) and verify signature
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
         auto amount = buyrambytes(8*1024);

         // Build authority with just one k1 key
         auto auth = authority{1,{{{0,eospub},1}},{},{}};

         // Issue to eosio the total amount of EOS to create this account
         // total = EOS to buy 8k of RAM + balance of account in eosio.unregd
         INLINE_ACTION_SENDER(call::token, issue)( N(eosio.token), {{N(eosio),N(active)}},
               {N(eosio), itr->balance + amount,""});

         // Create account with the same key for owner/active
         INLINE_ACTION_SENDER(call::eosio, newaccount)( N(eosio), {{N(eosio),N(active)}},
               {N(eosio), naccount, auth, auth});

         // Buy RAM for this account (8k)
         INLINE_ACTION_SENDER(call::eosio, buyram)( N(eosio), {{N(eosio),N(active)}},
               {N(eosio), naccount, amount});

         // Delegate bandwith
         INLINE_ACTION_SENDER(call::eosio, delegatebw)( N(eosio), {{N(eosio),N(active)}},
               {N(eosio), naccount, balances[0], balances[1], 1});

         // Transfer remaining if any (liquid EOS)
         if( balances[2] != asset(0) ) {
            INLINE_ACTION_SENDER(call::token, transfer)( N(eosio.token), {{N(eosio),N(active)}},
                  {N(eosio), naccount, balances[2], ""});
         }

         // Mark this address as "used"
         used_address.emplace( _self, [&]( auto& ua ) {
            ua.id = used_address.available_primary_key();
            ua.ethereum_address = {eth_address, eth_address+20};
         });
      }
};

}

EOSIO_ABI( eosio::claim, (regaccount) )
