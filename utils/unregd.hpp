#pragma once

namespace eosio {

   static uint8_t hex_char_to_uint(char character) {
      const int x = character;
      return (x <= 57) ? x - 48 : (x <= 70) ? (x - 65) + 0x0a : (x - 97) + 0x0a;
   }

   static key256 compute_ethereum_address_key256(const string& ethereum_address) {
      uint8_t ethereum_key[20];
      const char* characters = ethereum_address.c_str();

      // The ethereum address starts with 0x, let's skip those by starting at i = 2
      for (uint64_t i = 2; i < ethereum_address.length(); i += 2) {
         const uint64_t index = (i / 2) - 1;
         ethereum_key[index] = 16 * hex_char_to_uint(characters[i]) + hex_char_to_uint(characters[i + 1]);
      }

      const uint32_t* p32 = reinterpret_cast<const uint32_t*>(&ethereum_key);
      return key256::make_from_word_sequence<uint32_t>(p32[0], p32[1], p32[2], p32[3], p32[4]);
   }

   static key256 compute_ethereum_address_key256(uint8_t* ethereum_key) {
      const uint32_t* p32 = reinterpret_cast<const uint32_t*>(ethereum_key);
      return key256::make_from_word_sequence<uint32_t>(p32[0], p32[1], p32[2], p32[3], p32[4]);
   }

   struct address {
      uint64_t id;
      std::string ethereum_address;
      asset balance;

      uint64_t primary_key() const { return id; }
      key256 by_ethereum_address() const { return compute_ethereum_address_key256(ethereum_address); }

      EOSLIB_SERIALIZE(address, (id)(ethereum_address)(balance))
   };

   typedef eosio::multi_index<
      N(addresses), address,
      indexed_by<N(ethereum_address), const_mem_fun<address, key256, &address::by_ethereum_address>>
   > addresses_index;
}