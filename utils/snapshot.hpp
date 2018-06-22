#pragma once

namespace eosio {

   asset buyrambytes( uint32_t bytes ) {
      rammarket market(N(eosio),N(eosio));
      auto itr = market.find(S(4,RAMCORE));
      eosio_assert(itr != market.end(), "RAMCORE market not found");
      auto tmp = *itr;
      return tmp.convert( asset(bytes,S(0,RAM)), CORE_SYMBOL );
   }

   vector<asset> split_snapshot(const asset& balance) {
      if ( balance < asset(5000) ) {
         return {};
      }

      // everyone has minimum 0.25 EOS staked
      // some 10 EOS unstaked
      // the rest split between the two

      auto cpu = asset(2500);
      auto net = asset(2500);

      auto remainder = balance - cpu - net;

      if ( remainder <= asset(100000) ) /* 10.0 EOS */ {
         return {net, cpu, remainder};
      }

      remainder -= asset(100000); // keep them floating, unstaked

      auto first_half = remainder / 2;
      cpu += first_half;
      net += remainder - first_half;

      return {net, cpu, asset(100000)};
   }
}