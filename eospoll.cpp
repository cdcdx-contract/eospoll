/**
 * *  The apply() methods must have C calling convention so that the blockchain can lookup and
 * *  call these methods.
 * */
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/crypto.h>
#include <string>

extern "C" {
	struct transfer_args {
	   account_name    from;
	   account_name    to;
	   eosio::asset    quantity;
	   std::string     memo;
	};

	struct reveal_args {
	   uint64_t        tip;
	};

    struct offerbet {
    	uint64_t         id;
    	account_name     from;
    	account_name     to;
    	eosio::asset     quantity;
    	std::string      memo;
    	uint64_t primary_key() const {return id;}
    };
    typedef eosio::multi_index<N(offerbet), offerbet> offerbet_index;

	struct betnumber {
	   uint64_t          id;
	   std::string       offerbetid;
	   uint64_t          number;
	   uint64_t primary_key()const { return id; }
	};
	typedef eosio::multi_index< N(betnumber), betnumber> betnumber_index;

	struct globalindex {
	   uint64_t          id;
	   uint64_t          gindex;
	   uint64_t primary_key()const { return id; }
	};
	typedef eosio::multi_index< N(globalindex), globalindex> globalindex_index;

	struct betstate {
	   uint64_t          id;
	   uint64_t          round;
	   uint64_t          total;
	   eosio::asset      quantity;
	   uint64_t          result;
	   uint64_t primary_key()const { return id; }
	};
	typedef eosio::multi_index< N(betstate), betstate> betstate_index;

   /// The apply method implements the dispatch of events to this contract
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) {

	 //
	 auto _self = receiver;

	 //
	 globalindex_index globalindexs(_self, _self);
	 auto cur_globalindex_itr = globalindexs.find(N(bet));
	 if(cur_globalindex_itr == globalindexs.end()) {
		 cur_globalindex_itr = globalindexs.emplace(_self, [&](auto& info){
			 info.id       = N(bet);
			 info.gindex  = 0;
		 });
	 }

	 //
	 uint64_t bet_index = cur_globalindex_itr->gindex;

	 //
	 betstate_index betstates(_self, _self);
	 auto cur_betstate_itr = betstates.find(bet_index);
	 if(cur_betstate_itr == betstates.end()) {
		 cur_betstate_itr = betstates.emplace(_self, [&](auto& info){
			 info.id      = betstates.available_primary_key();
			 info.round   = bet_index;
			 info.total   = 0;
			 info.result  = 0xFFFFFFFFFF;
		 });
	 }

	 //
	 uint64_t bet_total = cur_betstate_itr->total;
	 uint64_t bet_quantity = cur_betstate_itr->quantity;

	 //
	 offerbet_index offerbets(_self, _self);

	 //
	 betnumber_index betnumbers(_self, _self);

	 //
	 if(code == N(eosio.token) && action == N(transfer)) {
		 transfer_args args = eosio::unpack_action_data<transfer_args>();
		 if(args.to != N(eospoll)) {
			 return;
		 }

		auto sym = args.quantity.symbol;
		eosio_assert( sym.is_valid(), "invalid symbol name" );
		eosio_assert( sym == CORE_SYMBOL, "invalid symbol name" );

		eosio_assert( args.memo.size() <= 256, "memo has more than 256 bytes" );
	    eosio_assert( args.quantity.is_valid(), "invalid quantity" );
	    eosio_assert( args.quantity.amount > 0, "must issue positive quantity" );

		 auto new_offerbet_itr = offerbets.emplace(_self, [&](auto& info){
			info.id           = offerbets.available_primary_key();
			info.from         = args.from;
			info.to           = args.to;
			info.quantity     = args.quantity;
			info.memo         = args.memo;
		 });

		 eosio::asset temp;
		 temp.set_amount(10000);

		 while(args.quantity > temp) {
			 args.quantity = args.quantity - temp;

			 auto new_betnumber_itr = betnumbers.emplace(_self, [&](auto& info){
				info.id           = betnumbers.available_primary_key();
				info.offerbetid   = new_offerbet_itr->id;
				info.number       = info.id;
			 });

			 //
			 bet_total ++;
			 bet_quantity += temp;
		 }

		 //
		 betstates.modify( cur_betstate_itr, 0, [&](auto& info) {
			 info.total = bet_total;
			 info.quantity = bet_quantity;
		 });
	 }

	 //
	 if(code == N(eospoll) && action == N(reveal)) {
		 reveal_args args = eosio::unpack_action_data<reveal_args>();

		 //
         uint32_t t_now = now();
         checksum256 hash;
         sha256((char*)(&t_now), sizeof(uint32_t), &hash);
         bet_result = (hash.hash[15]) % cur_betstate_itr->total;

         //
         cur_betstate_itr->result = bet_result;

         //
         betnumber_itr = betnumbers.find(bet_result);

         //
         offerbet_itr = offerbets.find(betnumber_itr->offerbetid);

         eosio::action(
            eosio::permission_level{ _self, N(active) },
            N(eosio.token), N(transfer),
            std::make_tuple(_self, offerbet_itr->to, cur_betstate_itr->quantity, offerbet_itr->memo)
         ).send();
	 }

	 //
     if(code == N(eospoll) && action == N(reset)) {
    	 {
    		 offerbet_index offetbets(_self, _self);
			 auto offetbets_itr = offetbets.begin();
			 while(offetbets_itr != offetbets.end()) {
				 offetbets_itr = offetbets.erase(offetbets_itr);
			 }
    	 }

    	 {
    		 betnumber_index betnumbers(_self, _self);
			 auto betnumbers_itr = betnumbers.begin();
			 while(betnumbers_itr != betnumbers.end()) {
				 betnumbers_itr = betnumbers.erase(betnumbers_itr);
			 }
    	 }

    	 {
    		 globalindex_index globalindexs(_self, _self);
			 auto globalindexs_itr = globalindexs.begin();
			 while(globalindexs_itr != globalindexs.end()) {
				 globalindexs_itr = globalindexs.erase(globalindexs_itr);
			 }
    	 }

    	 {
    		 betstate_index betstates(_self, _self);
			 auto betstates_itr = betstates.begin();
			 while(betstates_itr != betstates.end()) {
				 betstates_itr = betstates.erase(betstates_itr);
			 }
    	 }
     }
   }
} // extern "C"
